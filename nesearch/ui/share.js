/*
 * Social sharing for the nesearch catalog and the browser player.
 *
 * Aimed at an international audience: X, Facebook, Reddit, Telegram and Hacker
 * News, plus "copy link" and the native share sheet on mobile.
 *
 * The local UI runs on 127.0.0.1 where a shared link would be useless, so
 * sharing only turns on over https on a real host (GitHub Pages) or when the
 * build sets window.NESEARCH_SHARE_BASE.
 */
(function (root) {
  "use strict";

  var TARGETS = {
    x: {
      label: "X",
      url: function (link, text) {
        return "https://twitter.com/intent/tweet?url=" + encodeURIComponent(link) + "&text=" + encodeURIComponent(text);
      }
    },
    facebook: {
      label: "Facebook",
      url: function (link) {
        return "https://www.facebook.com/sharer/sharer.php?u=" + encodeURIComponent(link);
      }
    },
    reddit: {
      label: "Reddit",
      url: function (link, text) {
        return "https://www.reddit.com/submit?url=" + encodeURIComponent(link) + "&title=" + encodeURIComponent(text);
      }
    },
    telegram: {
      label: "Telegram",
      url: function (link, text) {
        return "https://t.me/share/url?url=" + encodeURIComponent(link) + "&text=" + encodeURIComponent(text);
      }
    },
    hackernews: {
      label: "HN",
      title: "Hacker News",
      url: function (link, text) {
        return "https://news.ycombinator.com/submitlink?u=" + encodeURIComponent(link) + "&t=" + encodeURIComponent(text);
      }
    }
  };

  function base() {
    if (root.NESEARCH_SHARE_BASE) {
      return String(root.NESEARCH_SHARE_BASE).replace(/[?#].*$/, "").replace(/index\.html$/, "");
    }
    if (!root.location || !root.location.hostname) return "";
    var loc = root.location;
    if (loc.protocol !== "https:") return "";
    if (/^(localhost|127\.0\.0\.1|\[::1\])$/.test(loc.hostname)) return "";
    return loc.origin + loc.pathname.replace(/[^/]*$/, "");
  }

  function enabled() {
    return base() !== "";
  }

  /* Per-game landing page, so link previews can carry that game's own image. */
  function gameUrl(id) {
    var rootUrl = base();
    if (!rootUrl) return "";
    return id ? rootUrl + "game/" + encodeURIComponent(id) + ".html" : rootUrl;
  }

  function shareText(title) {
    if (!title) return "nesearch — an open catalogue of NES homebrew games you can play in the browser";
    return title + " — NES homebrew you can play right in the browser";
  }

  /* Opens the network dialog in a popup. Returns false when unsupported. */
  function share(target, options) {
    var opts = options || {};
    var entry = TARGETS[target];
    if (!entry) return false;
    var link = opts.url || gameUrl(opts.id || "");
    if (!link) return false;
    var text = opts.text || shareText(opts.title);
    var target_url = entry.url(link, text);
    if (root.document) {
      /* A synthetic <a target="_blank"> click survives popup blockers, unlike
         window.open() from an async handler. */
      var anchor = root.document.createElement("a");
      anchor.href = target_url;
      anchor.target = "_blank";
      anchor.rel = "noopener noreferrer";
      anchor.style.display = "none";
      root.document.body.appendChild(anchor);
      anchor.click();
      anchor.remove();
      return true;
    }
    var popup = root.open(target_url, "_blank");
    if (popup) popup.opener = null;
    return true;
  }

  function copy(link) {
    if (!link) return Promise.resolve(false);
    if (root.navigator && root.navigator.clipboard && root.isSecureContext) {
      return root.navigator.clipboard.writeText(link).then(
        function () { return true; },
        function () { return legacyCopy(link); }
      );
    }
    return Promise.resolve(legacyCopy(link));
  }

  /* Works on http://127.0.0.1 previews where the async clipboard API is off. */
  function legacyCopy(link) {
    if (!root.document) return false;
    var area = root.document.createElement("textarea");
    area.value = link;
    area.setAttribute("readonly", "");
    area.style.position = "fixed";
    area.style.top = "-1000px";
    root.document.body.appendChild(area);
    area.select();
    var ok = false;
    try {
      ok = root.document.execCommand("copy");
    } catch (error) {
      ok = false;
    }
    area.remove();
    return ok;
  }

  function canNative() {
    return Boolean(root.navigator && typeof root.navigator.share === "function");
  }

  function native(options) {
    var opts = options || {};
    var link = opts.url || gameUrl(opts.id || "");
    if (!canNative() || !link) return Promise.resolve(false);
    return root.navigator
      .share({ title: opts.title || "nesearch", text: opts.text || shareText(opts.title), url: link })
      .then(function () { return true; }, function () { return false; });
  }

  var api = {
    targets: TARGETS,
    enabled: enabled,
    base: base,
    gameUrl: gameUrl,
    shareText: shareText,
    share: share,
    copy: copy,
    canNative: canNative,
    native: native
  };

  if (typeof module === "object" && module.exports) module.exports = api;
  else root.NEShare = api;
})(typeof globalThis !== "undefined" ? globalThis : this);
