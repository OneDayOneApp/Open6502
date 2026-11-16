/* ==========================================================================
   Open6502 — 芯片小课堂 deck
   Progressive enhancement for a static, dependency-free web presentation.
   ========================================================================== */
(() => {
  "use strict";

  const deck = document.querySelector("[data-deck]");
  if (!deck) return;

  const slides = Array.from(deck.querySelectorAll(".slide"));
  if (!slides.length) return;

  const stage = deck.querySelector("[data-deck-stage]");
  const counter = deck.querySelector("[data-deck-counter]");
  const progress = deck.querySelector("[data-deck-progress]");
  const caption = deck.querySelector("[data-deck-caption]");
  const noteText = deck.querySelector("[data-deck-note]");
  const notesPanel = deck.querySelector("[data-deck-notes]");
  const prevBtn = deck.querySelector("[data-deck-prev]");
  const nextBtn = deck.querySelector("[data-deck-next]");
  const overviewBtn = deck.querySelector("[data-deck-overview-open]");
  const notesBtn = deck.querySelector("[data-deck-notes-toggle]");
  const fullscreenBtn = deck.querySelector("[data-deck-fullscreen]");
  const overview = document.querySelector("[data-deck-overview]");
  const overviewGrid = document.querySelector("[data-deck-overview-grid]");
  const overviewClose = document.querySelector("[data-deck-overview-close]");

  const total = slides.length;
  const pad = (value) => String(value).padStart(2, "0");
  let index = 0;
  let overviewOpen = false;
  let lastFocus = null;

  const titleOf = (slide, i) => slide.dataset.slideTitle || `第 ${i + 1} 页`;

  function scrollToDeck() {
    const nav = document.querySelector(".site-nav");
    const offset = (nav ? nav.getBoundingClientRect().height : 0) + 20;
    const top = window.scrollY + deck.getBoundingClientRect().top - offset;
    window.scrollTo({ top: Math.max(0, top), behavior: "smooth" });
  }
  const noteOf = (slide) => {
    const note = slide.querySelector(".slide-note");
    return note ? note.textContent.replace(/\s+/g, " ").trim() : "";
  };

  function updateUI() {
    const slide = slides[index];
    if (counter) counter.textContent = `${pad(index + 1)} / ${pad(total)}`;
    if (progress) progress.style.width = `${((index + 1) / total) * 100}%`;
    if (caption) caption.textContent = titleOf(slide, index);
    if (noteText) noteText.textContent = noteOf(slide) || "这一页没有额外讲稿。";
    if (prevBtn) prevBtn.disabled = index === 0;
    if (nextBtn) nextBtn.disabled = index === total - 1;
    slides.forEach((item, i) => {
      item.setAttribute("aria-hidden", i === index ? "false" : "true");
    });
  }

  function setSlide(next, options = {}) {
    const target = Math.max(0, Math.min(total - 1, next));
    if (target === index && !options.force) {
      updateUI();
      if (options.hash !== false) {
        try {
          history.replaceState(null, "", `#slide-${index + 1}`);
        } catch (_) {
          /* history may be unavailable in a sandboxed preview */
        }
      }
      if (options.scroll) {
        scrollToDeck();
      }
      return;
    }

    slides[index].classList.remove("is-active");
    index = target;
    const slide = slides[index];
    slide.classList.add("is-active");
    slide.scrollTop = 0;
    updateUI();

    if (options.hash !== false) {
      try {
        history.replaceState(null, "", `#slide-${index + 1}`);
      } catch (_) {
        /* history may be unavailable in a sandboxed preview */
      }
    }

    if (options.scroll) {
      scrollToDeck();
    }
  }

  function next() {
    if (index < total - 1) setSlide(index + 1);
  }

  function previous() {
    if (index > 0) setSlide(index - 1);
  }

  function toggleNotes(force) {
    if (!notesPanel || !notesBtn) return;
    const open = typeof force === "boolean" ? force : notesPanel.hidden;
    notesPanel.hidden = !open;
    notesBtn.setAttribute("aria-pressed", String(open));
    notesBtn.textContent = open ? "收起讲稿" : "讲稿";
  }

  function buildOverview() {
    if (!overviewGrid || overviewGrid.dataset.ready === "true") return;
    slides.forEach((slide, i) => {
      const button = document.createElement("button");
      button.type = "button";
      button.innerHTML = `<b>${pad(i + 1)}</b><span></span>`;
      button.querySelector("span").textContent = titleOf(slide, i);
      button.addEventListener("click", () => {
        setSlide(i);
        closeOverview();
      });
      overviewGrid.appendChild(button);
    });
    overviewGrid.dataset.ready = "true";
  }

  function openOverview() {
    if (!overview) return;
    buildOverview();
    lastFocus = document.activeElement;
    overviewOpen = true;
    overview.hidden = false;
    document.body.classList.add("is-locked");
    overviewClose?.focus();
  }

  function closeOverview() {
    if (!overview) return;
    overviewOpen = false;
    overview.hidden = true;
    document.body.classList.remove("is-locked");
    if (lastFocus && typeof lastFocus.focus === "function") lastFocus.focus();
  }

  function setPresentation(on) {
    deck.classList.toggle("is-presenting", on);
    document.body.classList.toggle("is-locked", on);
    if (fullscreenBtn) {
      fullscreenBtn.setAttribute("aria-pressed", String(on));
      fullscreenBtn.textContent = on ? "退出全屏" : "全屏";
    }
  }

  async function toggleFullscreen() {
    if (deck.classList.contains("is-presenting")) {
      setPresentation(false);
      return;
    }
    try {
      if (!document.fullscreenElement) {
        await deck.requestFullscreen();
      } else {
        await document.exitFullscreen();
      }
    } catch (_) {
      /* iOS Safari and some embeds block the Fullscreen API; use a fixed layer. */
      setPresentation(true);
    }
  }

  function syncFullscreenButton() {
    if (!fullscreenBtn) return;
    const on = document.fullscreenElement === deck || deck.classList.contains("is-presenting");
    fullscreenBtn.setAttribute("aria-pressed", String(on));
    fullscreenBtn.textContent = on ? "退出全屏" : "全屏";
  }

  prevBtn?.addEventListener("click", previous);
  nextBtn?.addEventListener("click", next);
  overviewBtn?.addEventListener("click", openOverview);
  overviewClose?.addEventListener("click", closeOverview);
  notesBtn?.addEventListener("click", () => toggleNotes());
  fullscreenBtn?.addEventListener("click", toggleFullscreen);
  document.addEventListener("fullscreenchange", syncFullscreenButton);

  overview?.addEventListener("click", (event) => {
    if (event.target === overview) closeOverview();
  });

  document.addEventListener("keydown", (event) => {
    if (event.defaultPrevented) return;

    if (event.key === "Escape") {
      if (overviewOpen) {
        event.preventDefault();
        closeOverview();
        return;
      }
      if (deck.classList.contains("is-presenting")) {
        event.preventDefault();
        setPresentation(false);
      }
      return;
    }

    if (overviewOpen) return;

    const target = event.target;
    const interactive =
      target instanceof Element &&
      target.closest("button, a, input, textarea, select, summary, [contenteditable]");
    if (interactive && (event.key === " " || event.key === "Enter")) return;

    switch (event.key) {
      case "ArrowRight":
      case "PageDown":
      case " ":
        event.preventDefault();
        next();
        break;
      case "ArrowLeft":
      case "PageUp":
        event.preventDefault();
        previous();
        break;
      case "Home":
        event.preventDefault();
        setSlide(0);
        break;
      case "End":
        event.preventDefault();
        setSlide(total - 1);
        break;
      case "f":
      case "F":
        event.preventDefault();
        toggleFullscreen();
        break;
      case "o":
      case "O":
        event.preventDefault();
        openOverview();
        break;
      case "n":
      case "N":
        event.preventDefault();
        toggleNotes();
        break;
      default:
        break;
    }
  });

  /* Focus trap for the overview dialog. */
  overview?.addEventListener("keydown", (event) => {
    if (event.key !== "Tab") return;
    const focusable = Array.from(
      overview.querySelectorAll("button, [href], input, select, textarea, [tabindex]:not([tabindex='-1'])")
    ).filter((el) => !el.disabled && el.offsetParent !== null);
    if (!focusable.length) return;
    const first = focusable[0];
    const last = focusable[focusable.length - 1];
    if (event.shiftKey && document.activeElement === first) {
      event.preventDefault();
      last.focus();
    } else if (!event.shiftKey && document.activeElement === last) {
      event.preventDefault();
      first.focus();
    }
  });

  /* Touch swipe. */
  let touchStartX = 0;
  let touchStartY = 0;
  stage?.addEventListener(
    "touchstart",
    (event) => {
      if (event.touches.length !== 1) return;
      touchStartX = event.touches[0].clientX;
      touchStartY = event.touches[0].clientY;
    },
    { passive: true }
  );
  stage?.addEventListener(
    "touchend",
    (event) => {
      if (overviewOpen || event.changedTouches.length !== 1) return;
      const dx = event.changedTouches[0].clientX - touchStartX;
      const dy = event.changedTouches[0].clientY - touchStartY;
      if (Math.abs(dx) < 48 || Math.abs(dx) < Math.abs(dy)) return;
      if (dx < 0) next();
      else previous();
    },
    { passive: true }
  );

  /* In-slide quiz toggles. */
  document.querySelectorAll("[data-quiz-toggle]").forEach((button) => {
    button.addEventListener("click", () => {
      const answer = document.getElementById(button.getAttribute("aria-controls"));
      if (!answer) return;
      const expanded = button.getAttribute("aria-expanded") === "true";
      button.setAttribute("aria-expanded", String(!expanded));
      answer.hidden = expanded;
      button.textContent = expanded ? "揭晓答案" : "收起答案";
    });
  });

  /* Start / resume links. */
  document.querySelectorAll("[data-start-deck]").forEach((link) => {
    link.addEventListener("click", (event) => {
      event.preventDefault();
      setSlide(0, { scroll: true });
    });
  });

  /* Deep link: #slide-7. */
  const fromHash = () => {
    const match = window.location.hash.match(/^#slide-(\d+)$/);
    if (!match) return 0;
    return Math.max(0, Math.min(total - 1, Number(match[1]) - 1));
  };

  window.addEventListener("hashchange", () => {
    setSlide(fromHash(), { hash: false, force: true });
  });

  slides.forEach((slide, i) => {
    slide.classList.toggle("is-active", i === fromHash());
    slide.setAttribute("aria-hidden", i === fromHash() ? "false" : "true");
  });
  index = fromHash();
  updateUI();
  syncFullscreenButton();
})();
