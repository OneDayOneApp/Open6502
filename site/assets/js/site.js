/* ==========================================================================
   Open6502 — progressive enhancement
   No dependencies. Everything below is optional: the pages read fine without it.
   ========================================================================== */
(() => {
  "use strict";

  const $ = (sel, root = document) => root.querySelector(sel);
  const $$ = (sel, root = document) => Array.from(root.querySelectorAll(sel));
  const prefersReduced = window.matchMedia("(prefers-reduced-motion: reduce)").matches;

  /* ---------- Toast ---------- */
  let toastEl = null;
  let toastTimer = 0;
  function toast(message) {
    if (!toastEl) {
      toastEl = document.createElement("div");
      toastEl.className = "toast";
      toastEl.setAttribute("role", "status");
      toastEl.setAttribute("aria-live", "polite");
      document.body.appendChild(toastEl);
    }
    toastEl.textContent = message;
    toastEl.classList.add("is-in");
    clearTimeout(toastTimer);
    toastTimer = window.setTimeout(() => toastEl.classList.remove("is-in"), 1800);
  }

  /* ---------- Clipboard ---------- */
  function copyFallback(text) {
    // Synchronous fallback keeps the user activation from the click event.
    const area = document.createElement("textarea");
    area.value = text;
    area.setAttribute("readonly", "");
    area.style.cssText = "position:fixed;top:-1000px;left:-1000px;opacity:0";
    document.body.appendChild(area);
    area.select();
    let ok = false;
    try {
      ok = document.execCommand("copy");
    } catch (_) {
      ok = false;
    }
    document.body.removeChild(area);
    return ok;
  }

  async function copyText(text) {
    if (copyFallback(text)) return true;
    if (navigator.clipboard && window.isSecureContext) {
      try {
        await navigator.clipboard.writeText(text);
        return true;
      } catch (_) {
        /* the toast below reports the failure */
      }
    }
    return false;
  }

  document.addEventListener("click", async (event) => {
    const button = event.target.closest("[data-copy]");
    if (!button) return;
    let text = button.getAttribute("data-copy");
    if (!text) {
      const block = button.closest(".cmd, .code, .sha-block");
      text = block ? (block.querySelector("pre") || block).innerText : "";
    }
    const ok = await copyText(text.trim());
    if (!ok) {
      toast("复制失败，请手动选择文本");
      return;
    }
    const label = $(".copy-label", button);
    if (label) {
      const previous = label.textContent;
      label.textContent = "已复制";
      button.classList.add("is-copied");
      window.setTimeout(() => {
        label.textContent = previous;
        button.classList.remove("is-copied");
      }, 1600);
    } else {
      button.classList.add("is-copied");
      window.setTimeout(() => button.classList.remove("is-copied"), 1600);
    }
    toast("已复制到剪贴板");
  });

  /* ---------- Mobile navigation ---------- */
  const navToggle = $("[data-nav-toggle]");
  const navMobile = $(".nav-mobile");
  if (navToggle && navMobile) {
    const setNav = (open) => {
      navMobile.classList.toggle("is-open", open);
      navToggle.setAttribute("aria-expanded", String(open));
      navToggle.setAttribute("aria-label", open ? "关闭导航菜单" : "打开导航菜单");
    };
    navToggle.addEventListener("click", () =>
      setNav(!navMobile.classList.contains("is-open"))
    );
    $$("a", navMobile).forEach((a) => a.addEventListener("click", () => setNav(false)));
    document.addEventListener("keydown", (e) => {
      if (e.key === "Escape") setNav(false);
    });
    window.addEventListener("resize", () => {
      if (window.innerWidth > 960) setNav(false);
    });
  }

  /* ---------- Operating-system tabs (quickstart) ---------- */
  const osTabs = $$("[data-os-tab]");
  if (osTabs.length) {
    const panels = $$("[data-os-panel]");
    const activate = (os) => {
      osTabs.forEach((tab) => {
        const selected = tab.dataset.osTab === os;
        tab.setAttribute("aria-selected", String(selected));
        tab.tabIndex = selected ? 0 : -1;
      });
      panels.forEach((panel) => {
        panel.hidden = panel.dataset.osPanel !== os;
      });
    };
    const guess = () => {
      const saved = localStorage.getItem("open6502:os");
      if (saved === "mac" || saved === "linux") return saved;
      const ua = navigator.userAgent;
      if (/Linux/i.test(ua) && !/Android/i.test(ua)) return "linux";
      return "mac";
    };
    osTabs.forEach((tab, index) =>
      tab.addEventListener("click", () => {
        const os = tab.dataset.osTab;
        activate(os);
        try {
          localStorage.setItem("open6502:os", os);
        } catch (_) {}
      })
    );
    osTabs.forEach((tab, index) => {
      tab.addEventListener("keydown", (event) => {
        if (event.key !== "ArrowRight" && event.key !== "ArrowLeft") return;
        event.preventDefault();
        const delta = event.key === "ArrowRight" ? 1 : -1;
        const next = osTabs[(index + delta + osTabs.length) % osTabs.length];
        next.focus();
        next.click();
      });
    });
    activate(guess());
  }

  /* ---------- Hero frame switcher ---------- */
  const frameTabs = $$("[data-frame-tab]");
  if (frameTabs.length) {
    const image = $("[data-frame-image]");
    const caption = $("[data-frame-caption]");
    const hash = $("[data-frame-hash]");
    frameTabs.forEach((tab) =>
      tab.addEventListener("click", () => {
        frameTabs.forEach((item) =>
          item.setAttribute("aria-pressed", String(item === tab))
        );
        if (image) {
          image.src = tab.dataset.src;
          image.alt = tab.dataset.alt || image.alt;
        }
        if (caption) caption.textContent = tab.dataset.caption || caption.textContent;
        if (hash) hash.textContent = tab.dataset.hash || hash.textContent;
      })
    );
  }

  /* ---------- Checklist with local persistence ---------- */
  const checklist = $("[data-checklist]");
  if (checklist) {
    const boxes = $$('input[type="checkbox"]', checklist);
    const fill = $("[data-checklist-bar]");
    const count = $("[data-checklist-count]");
    const key = "open6502:checklist";
    let saved = {};
    try {
      saved = JSON.parse(localStorage.getItem(key) || "{}");
    } catch (_) {
      saved = {};
    }
    const render = () => {
      const done = boxes.filter((box) => box.checked).length;
      if (fill) fill.style.width = `${(done / boxes.length) * 100}%`;
      if (count) count.textContent = `${done} / ${boxes.length}`;
      boxes.forEach((box) => box.closest(".check")?.classList.toggle("is-done", box.checked));
    };
    boxes.forEach((box, index) => {
      box.checked = Boolean(saved[index]);
      box.addEventListener("change", () => {
        saved[index] = box.checked;
        try {
          localStorage.setItem(key, JSON.stringify(saved));
        } catch (_) {}
        render();
      });
    });
    render();
  }

  /* ---------- NEMU / RTL compare slider ---------- */
  const range = $("[data-compare-range]");
  if (range) {
    const stage = range.closest(".compare");
    const update = () => {
      stage.style.setProperty("--pos", `${range.value}%`);
      range.setAttribute(
        "aria-valuetext",
        `${range.value}% 显示 RTL，${100 - Number(range.value)}% 显示 NEMU`
      );
    };
    range.addEventListener("input", update);
    update();
  }

  /* ---------- Scroll reveal ---------- */
  const reveals = $$(".reveal");
  if (reveals.length) {
    if (prefersReduced || !("IntersectionObserver" in window)) {
      reveals.forEach((el) => el.classList.add("is-in"));
    } else {
      const observer = new IntersectionObserver(
        (entries) => {
          entries.forEach((entry) => {
            if (entry.isIntersecting) {
              entry.target.classList.add("is-in");
              observer.unobserve(entry.target);
            }
          });
        },
        { rootMargin: "0px 0px -8% 0px", threshold: 0.08 }
      );
      reveals.forEach((el) => observer.observe(el));
    }
  }

  /* ---------- Back to top + reading progress ---------- */
  const toTop = $("[data-to-top]");
  const readingBar = $("[data-reading-bar]");
  const onScroll = () => {
    if (toTop) toTop.classList.toggle("is-in", window.scrollY > 640);
    if (readingBar) {
      const max = document.documentElement.scrollHeight - window.innerHeight;
      const ratio = max > 0 ? Math.min(1, window.scrollY / max) : 0;
      readingBar.style.width = `${ratio * 100}%`;
    }
  };
  window.addEventListener("scroll", onScroll, { passive: true });
  onScroll();
  if (toTop) {
    toTop.addEventListener("click", () => {
      window.scrollTo({ top: 0, behavior: prefersReduced ? "auto" : "smooth" });
    });
  }

  /* ---------- Docs mobile drawer ---------- */
  const docsMenu = $("[data-docs-menu]");
  const docsSidebar = $(".docs-sidebar");
  const docsBackdrop = $("[data-docs-backdrop]");
  if (docsMenu && docsSidebar && docsBackdrop) {
    const setDrawer = (open) => {
      docsSidebar.classList.toggle("is-open", open);
      docsBackdrop.classList.toggle("is-open", open);
      docsMenu.setAttribute("aria-expanded", String(open));
      document.body.classList.toggle("is-locked", open);
      if (open) {
        window.setTimeout(() => $("[data-docs-close]", docsSidebar)?.focus(), 60);
      } else if (document.activeElement && docsSidebar.contains(document.activeElement)) {
        docsMenu.focus();
      }
    };
    docsMenu.addEventListener("click", () =>
      setDrawer(!docsSidebar.classList.contains("is-open"))
    );
    docsBackdrop.addEventListener("click", () => setDrawer(false));
    $$("[data-docs-close]").forEach((button) =>
      button.addEventListener("click", () => setDrawer(false))
    );
    $$("a", docsSidebar).forEach((a) => a.addEventListener("click", () => setDrawer(false)));
    document.addEventListener("keydown", (e) => {
      if (e.key === "Escape") setDrawer(false);
    });
  }

  /* ---------- Docs scroll spy ---------- */
  const docSections = $$(".doc-section");
  const spyLinks = $$("[data-spy]");
  if (docSections.length && spyLinks.length && "IntersectionObserver" in window) {
    const linkFor = (id) => spyLinks.filter((link) => link.getAttribute("href") === `#${id}`);
    const activate = (id) => {
      spyLinks.forEach((link) => {
        const active = link.getAttribute("href") === `#${id}`;
        link.classList.toggle("is-active", active);
        if (active) link.setAttribute("aria-current", "location");
        else link.removeAttribute("aria-current");
      });
    };
    const spy = new IntersectionObserver(
      (entries) => {
        const visible = entries
          .filter((entry) => entry.isIntersecting)
          .sort((a, b) => a.boundingClientRect.top - b.boundingClientRect.top);
        if (visible[0]) activate(visible[0].target.id);
      },
      { rootMargin: "-18% 0px -66% 0px", threshold: 0 }
    );
    docSections.forEach((section) => spy.observe(section));
    // Make sure a hash on load is reflected immediately.
    if (location.hash) activate(location.hash.slice(1));
    // Fallback highlight for links to sections not yet observed.
    spyLinks.forEach((link) =>
      link.addEventListener("click", () => {
        const id = link.getAttribute("href")?.slice(1);
        if (id) activate(id);
      })
    );
    void linkFor;
  }

  /* ---------- Docs search ---------- */
  const searchOverlay = $("[data-search-overlay]");
  if (searchOverlay && docSections.length) {
    const field = $("[data-search-field]", searchOverlay);
    const results = $("[data-search-results]", searchOverlay);
    const openers = $$("[data-search-open]");
    const closeBtn = $("[data-search-close]", searchOverlay);
    let activeIndex = -1;
    let matches = [];

    const index = docSections.map((section, order) => {
      const heading = $("h2", section);
      const titleNode = heading ? heading.lastElementChild || heading : null;
      const title = (titleNode?.textContent || section.id).replace(/\s+/g, " ").trim();
      const keywords = section.dataset.keywords || "";
      const clone = section.cloneNode(true);
      clone.querySelectorAll(".ascii").forEach((node) => node.remove());
      const body = clone.textContent.replace(/\s+/g, " ").trim();
      return {
        id: section.id,
        group: section.dataset.group || "",
        title,
        keywords,
        text: `${title} ${keywords} ${body}`,
        body,
        order,
      };
    });

    const escapeHtml = (value) =>
      value.replace(/[&<>"']/g, (char) => ({
        "&": "&amp;",
        "<": "&lt;",
        ">": "&gt;",
        '"': "&quot;",
        "'": "&#39;",
      })[char]);

    const highlight = (value, query) => {
      const escaped = escapeHtml(value);
      if (!query) return escaped;
      const terms = query
        .split(/\s+/)
        .filter(Boolean)
        .map((term) => term.replace(/[.*+?^${}()|[\]\\]/g, "\\$&"));
      if (!terms.length) return escaped;
      return escaped.replace(new RegExp(`(${terms.join("|")})`, "gi"), "<mark>$1</mark>");
    };

    const snippet = (item, query) => {
      if (!query) return item.body.slice(0, 84);
      const lower = item.body.toLowerCase();
      const firstTerm = query.trim().split(/\s+/)[0].toLowerCase();
      const pos = lower.indexOf(firstTerm);
      const start = Math.max(0, pos - 34);
      const cut = item.body.slice(start, start + 118);
      return `${start > 0 ? "…" : ""}${cut}${start + 118 < item.body.length ? "…" : ""}`;
    };

    const render = (query) => {
      const q = query.trim().toLowerCase();
      const terms = q.split(/\s+/).filter(Boolean);
      const rank = (item) => {
        const title = item.title.toLowerCase();
        const keywords = item.keywords.toLowerCase();
        if (terms.every((term) => title.includes(term))) return 0;
        if (terms.every((term) => keywords.includes(term))) return 1;
        return 2;
      };
      matches = !q
        ? index.slice(0, 8)
        : index
            .filter((item) => terms.every((term) => item.text.toLowerCase().includes(term)))
            .sort((a, b) => rank(a) - rank(b) || a.order - b.order)
            .slice(0, 10);
      activeIndex = matches.length ? 0 : -1;
      results.innerHTML = matches.length
        ? matches
            .map(
              (item, i) => `
              <a class="search-result${i === 0 ? " is-active" : ""}" href="#${item.id}" data-index="${i}">
                <span class="search-result-title">
                  <span class="path">${escapeHtml(item.group)}</span>
                  ${highlight(item.title, query)}
                </span>
                <span class="search-result-snippet">${highlight(snippet(item, query), query)}</span>
              </a>`
            )
            .join("")
        : `<div class="search-empty">没有找到相关章节，试试 “PPU”、“Verilator” 或 “Klaus”。</div>`;
    };

    let lastFocus = null;
    const open = () => {
      lastFocus = document.activeElement;
      searchOverlay.classList.add("is-open");
      document.body.classList.add("is-locked");
      field.value = "";
      render("");
      window.setTimeout(() => field.focus(), 20);
    };
    const close = () => {
      searchOverlay.classList.remove("is-open");
      document.body.classList.remove("is-locked");
      activeIndex = -1;
      if (lastFocus && typeof lastFocus.focus === "function") lastFocus.focus();
    };
    const move = (delta) => {
      if (!matches.length) return;
      activeIndex = (activeIndex + delta + matches.length) % matches.length;
      $$(".search-result", results).forEach((el, i) => {
        el.classList.toggle("is-active", i === activeIndex);
        if (i === activeIndex) el.scrollIntoView({ block: "nearest" });
      });
    };

    openers.forEach((button) => button.addEventListener("click", open));
    closeBtn?.addEventListener("click", close);
    searchOverlay.addEventListener("click", (event) => {
      if (event.target === searchOverlay) close();
    });
    field.addEventListener("input", () => render(field.value));
    field.addEventListener("keydown", (event) => {
      if (event.key === "ArrowDown") {
        event.preventDefault();
        move(1);
      } else if (event.key === "ArrowUp") {
        event.preventDefault();
        move(-1);
      } else if (event.key === "Enter" && activeIndex >= 0 && matches[activeIndex]) {
        event.preventDefault();
        location.hash = `#${matches[activeIndex].id}`;
        close();
      }
    });
    results.addEventListener("click", (event) => {
      if (event.target.closest(".search-result")) close();
    });
    document.addEventListener("keydown", (event) => {
      const typing = /^(INPUT|TEXTAREA|SELECT)$/.test(document.activeElement?.tagName || "");
      if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === "k") {
        event.preventDefault();
        searchOverlay.classList.contains("is-open") ? close() : open();
      } else if (event.key === "/" && !typing) {
        event.preventDefault();
        open();
      } else if (event.key === "Escape" && searchOverlay.classList.contains("is-open")) {
        close();
      }
    });
  } else {
    // Landing page: still support ⌘K by sending readers to the docs search.
    const searchOpeners = $$("[data-search-open]");
    searchOpeners.forEach((button) =>
      button.addEventListener("click", () => {
        location.href = button.dataset.href || "docs.html#search";
      })
    );
  }

  /* ---------- Platform shortcut labels ---------- */
  const isMac = /Mac|iPhone|iPad|iPod/.test(navigator.platform || navigator.userAgent);
  if (!isMac) {
    $$("[data-shortcut]").forEach((el) => {
      el.textContent = "Ctrl K";
    });
  }

  /* ---------- Footer year ---------- */
  $$("[data-year]").forEach((el) => {
    el.textContent = String(new Date().getFullYear());
  });
})();
