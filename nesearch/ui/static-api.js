/*
 * Static backend for the GitHub Pages demo.
 *
 * The local UI talks to `nesearch ui` over `/api/v1/*`. On Pages there is no
 * server, so this file patches `window.fetch` and answers the same routes from
 * a pre-built `data/catalog.json`: browsing, search, detail, sources, plus a
 * best-effort download (fetch + browser save) and a link to the WebAssembly
 * player. Nothing here is loaded unless a page sets `window.NESEARCH_STATIC`.
 *
 * The query engine mirrors `CatalogService::search` in src/catalog.cpp so the
 * static demo filters and sorts exactly like the CLI and the local UI.
 */
(function (root, factory) {
  if (typeof module === "object" && module.exports) {
    module.exports = factory(root);
  } else {
    root.NESearchStatic = factory(root);
  }
})(typeof globalThis !== "undefined" ? globalThis : this, function (root) {
  "use strict";

  var API_PREFIX = "/api/v1/";
  var DOWNLOAD_DIR_LABEL = "browser download";

  function lower(value) {
    return String(value == null ? "" : value).toLowerCase();
  }

  function trim(value) {
    return String(value == null ? "" : value).trim();
  }

  function splitTokens(query) {
    return lower(trim(query)).split(/\s+/).filter(Boolean);
  }

  /* Mirrors relevance() in src/catalog.cpp. */
  function relevance(game, query) {
    if (!query) return 0;
    var id = lower(game.id);
    var title = lower(game.title);
    var author = lower(game.author);
    if (id === query) return 100;
    if (title.indexOf(query) === 0) return 80;
    if (title.indexOf(query) !== -1) return 60;
    if (author.indexOf(query) !== -1) return 40;
    if (lower(game.description).indexOf(query) !== -1) return 20;
    return 10;
  }

  function mapperMatches(game, mapper) {
    var name = game.mapper_name || "";
    if (mapper === "NROM") return name === "NROM";
    if (mapper === "MMC1") return name === "MMC1";
    if (mapper === "CNROM") return name === "CNROM";
    if (mapper === "other") {
      return !(name === "NROM" || name === "MMC1" || name === "CNROM" || name === "");
    }
    return true;
  }

  /* Returns the filtered + sorted list; callers slice for pagination. */
  function filterGames(games, options) {
    var opts = options || {};
    var query = lower(trim(opts.q));
    var tokens = splitTokens(opts.q);
    var license = opts.license || "all";
    var source = opts.source || "all";
    var mapper = opts.mapper || "all";
    var sort = opts.sort || "relevance";

    var result = games.filter(function (game) {
      var gameLicense = lower(game.license);
      if (license === "open" && gameLicense === "unknown") return false;
      if (license === "unknown" && gameLicense !== "unknown") return false;
      if (source !== "all" && source && game.source !== source) return false;
      if (!mapperMatches(game, mapper)) return false;
      if (tokens.length) {
        var haystack = lower(
          [game.id, game.title, game.author, game.description, game.license, game.source, game.mapper_name].join(" ")
        );
        for (var i = 0; i < tokens.length; i += 1) {
          if (haystack.indexOf(tokens[i]) === -1) return false;
        }
      }
      return true;
    });

    if (sort === "title") {
      result.sort(function (a, b) { return lower(a.title) < lower(b.title) ? -1 : lower(a.title) > lower(b.title) ? 1 : 0; });
    } else if (sort === "size") {
      result.sort(function (a, b) { return (b.size || 0) - (a.size || 0); });
    } else if (sort === "newest") {
      result.sort(function (a, b) { return (b.year || 0) - (a.year || 0); });
    } else if (query) {
      result.sort(function (a, b) {
        var diff = relevance(b, query) - relevance(a, query);
        if (diff !== 0) return diff;
        return lower(a.title) < lower(b.title) ? -1 : lower(a.title) > lower(b.title) ? 1 : 0;
      });
    } else {
      result.sort(function (a, b) { return lower(a.title) < lower(b.title) ? -1 : lower(a.title) > lower(b.title) ? 1 : 0; });
    }
    return result;
  }

  function paginate(list, limit, offset) {
    var start = Math.max(0, Number(offset) || 0);
    var size = Number(limit);
    if (!Number.isFinite(size) || size < 0) size = 0;
    if (start >= list.length) return [];
    return size === 0 ? list.slice(start) : list.slice(start, start + size);
  }

  function queryGames(games, options) {
    var opts = options || {};
    var limit = opts.limit == null ? 500 : Number(opts.limit);
    var offset = opts.offset == null ? 0 : Number(opts.offset);
    var matches = filterGames(games, opts);
    return { games: paginate(matches, limit, offset), total: matches.length, limit: limit, offset: offset };
  }

  function formatBytes(bytes) {
    var value = Number(bytes) || 0;
    if (value < 1024) return value + " B";
    var units = ["KiB", "MiB", "GiB"];
    var index = -1;
    do {
      value /= 1024;
      index += 1;
    } while (value >= 1024 && index < units.length - 1);
    return (value >= 10 ? value.toFixed(0) : value.toFixed(1)) + " " + units[index];
  }

  function sha256Hex(buffer) {
    if (!root.crypto || !root.crypto.subtle) return Promise.resolve("");
    return root.crypto.subtle.digest("SHA-256", buffer).then(function (digest) {
      var bytes = new Uint8Array(digest);
      var out = "";
      for (var i = 0; i < bytes.length; i += 1) out += bytes[i].toString(16).padStart(2, "0");
      return out;
    });
  }

  function createBackend(payload, options) {
    var opts = options || {};
    var fetchImpl = opts.fetchImpl || (root.fetch ? root.fetch.bind(root) : null);
    var dataUrl = opts.dataUrl || "data/catalog.json";
    var games = Array.isArray(payload.games) ? payload.games.slice() : [];
    var byId = new Map(games.map(function (game) { return [game.id, game]; }));
    var sources = Array.isArray(payload.sources) ? payload.sources.slice() : [];
    var downloads = new Map();
    var library = new Map();
    var config = {
      download_dir: DOWNLOAD_DIR_LABEL,
      jobs: 4,
      proxy: "",
      ui_port: 0,
      config_path: "static demo (GitHub Pages)",
      archive_org_enabled: false,
      archive_org_query: "",
      archive_org_limit: 0
    };

    function ok(body) { return { status: 200, body: body }; }
    function fail(status, message) { return { status: status, body: { error: message, status: status } }; }

    function statusPayload() {
      var active = 0;
      downloads.forEach(function (item) {
        if (item.state === "queued" || item.state === "downloading" || item.state === "verifying") active += 1;
      });
      return {
        version: payload.generator || "static",
        games: games.length,
        sources: sources.length,
        active_downloads: active,
        fetched_at: payload.fetched_at || payload.generated_at || "",
        download_dir: config.download_dir
      };
    }

    function downloadList() {
      var list = [];
      downloads.forEach(function (item) {
        list.push({
          id: item.id,
          title: item.title,
          state: item.state,
          error: item.error || "",
          path: item.path || "",
          received: item.received || 0,
          total: item.total || 0,
          speed: item.speed || 0,
          progress: item.progress || 0
        });
      });
      return list;
    }

    function libraryList() {
      var list = [];
      library.forEach(function (entry) { list.push(entry); });
      return list;
    }

    function saveToDisk(blob, filename) {
      if (typeof document === "undefined") return;
      var link = document.createElement("a");
      link.href = URL.createObjectURL(blob);
      link.download = filename;
      link.rel = "noopener";
      document.body.appendChild(link);
      link.click();
      link.remove();
      setTimeout(function () { URL.revokeObjectURL(link.href); }, 60000);
    }

    function playHref(game) {
      var base = opts.playUrl || "play.html";
      var query = "?rom=" + encodeURIComponent(game.url || "") + "&title=" + encodeURIComponent(game.title || game.id);
      return base + query;
    }

    function startDownload(id) {
      var game = byId.get(id);
      if (!game) return;
      if (!fetchImpl) return;
      var item = downloads.get(id);
      if (item && (item.state === "downloading" || item.state === "queued")) return;

      item = {
        id: id,
        title: game.title || id,
        state: "downloading",
        error: "",
        path: "",
        received: 0,
        total: game.size || 0,
        progress: 0,
        speed: 0,
        controller: null
      };
      downloads.set(id, item);

      var controller = typeof AbortController !== "undefined" ? new AbortController() : null;
      item.controller = controller;
      var started = Date.now();

      Promise.resolve()
        .then(function () {
          return fetchImpl(game.url, { mode: "cors", signal: controller ? controller.signal : undefined });
        })
        .then(function (response) {
          if (!response.ok) throw new Error("HTTP " + response.status);
          var declared = Number(response.headers.get("content-length")) || game.size || 0;
          if (!response.body || !response.body.getReader) {
            return response.arrayBuffer().then(function (buffer) {
              item.received = buffer.byteLength;
              item.total = buffer.byteLength;
              item.progress = 1;
              return buffer;
            });
          }
          var reader = response.body.getReader();
          var chunks = [];
          function pump() {
            return reader.read().then(function (step) {
              if (step.done) {
                var size = chunks.reduce(function (sum, chunk) { return sum + chunk.byteLength; }, 0);
                var merged = new Uint8Array(size);
                var offset = 0;
                chunks.forEach(function (chunk) {
                  merged.set(chunk, offset);
                  offset += chunk.byteLength;
                });
                return merged.buffer;
              }
              chunks.push(step.value);
              item.received += step.value.byteLength;
              item.total = declared || item.received;
              item.progress = item.total ? Math.min(1, item.received / item.total) : 0;
              var seconds = Math.max(0.05, (Date.now() - started) / 1000);
              item.speed = item.received / 1024 / 1024 / seconds;
              return pump();
            });
          }
          return pump();
        })
        .then(function (buffer) {
          var filename = game.file || (id + ".nes");
          item.state = "verifying";
          return sha256Hex(buffer).then(function (digest) {
            var blob = new Blob([buffer], { type: "application/octet-stream" });
            saveToDisk(blob, filename);
            item.state = "done";
            item.progress = 1;
            item.received = blob.size;
            item.total = blob.size;
            item.path = DOWNLOAD_DIR_LABEL + " · " + filename;
            library.set(id, {
              id: id,
              title: game.title || id,
              path: item.path,
              source: game.source || "",
              sha256: digest || game.sha256 || "",
              size: blob.size,
              size_label: formatBytes(blob.size),
              mapper: game.mapper_name || "",
              mapper_num: Number.isFinite(game.mapper_num) ? game.mapper_num : -1,
              downloaded_at: new Date().toISOString()
            });
          });
        })
        .catch(function (error) {
          if (controller && controller.signal.aborted) {
            item.state = "paused";
            return;
          }
          item.state = "error";
          item.error = String((error && error.message) || error);
        });
    }

    function gamePayload(game) {
      if (!game) return null;
      return {
        id: game.id,
        title: game.title,
        author: game.author,
        license: game.license,
        license_kind: game.license_kind,
        description: game.description,
        page_url: game.page_url,
        source: game.source,
        source_id: game.source_id,
        file: game.file,
        url: game.url,
        sha256: game.sha256,
        screenshot: game.screenshot,
        size: game.size,
        size_label: game.size_label || formatBytes(game.size),
        mapper: game.mapper,
        mapper_name: game.mapper_name,
        mapper_num: game.mapper_num,
        prg_kb: game.prg_kb,
        chr_kb: game.chr_kb,
        mirroring: game.mirroring,
        battery: game.battery,
        year: game.year,
        date: game.date
      };
    }

    function handle(pathname, params, init) {
      var method = ((init && init.method) || "GET").toUpperCase();
      var body = {};
      if (init && init.body) {
        try {
          body = typeof init.body === "string" ? JSON.parse(init.body) : init.body;
        } catch (error) {
          body = {};
        }
      }
      var route = pathname.slice(pathname.indexOf(API_PREFIX) + API_PREFIX.length);

      if (route === "status") return ok(statusPayload());
      if (route === "emulators") {
        return ok({
          emulators: [
            { id: "gem6502", name: "GEM6502", path: "WebAssembly (built into this page)", available: true },
            { id: "nemu", name: "NEMU", path: "", available: false }
          ]
        });
      }
      if (route === "games") {
        return ok(queryGames(games, {
          q: params.get("q") || "",
          license: params.get("license") || "all",
          source: params.get("source") || "all",
          mapper: params.get("mapper") || "all",
          sort: params.get("sort") || "relevance",
          limit: params.get("limit") == null ? 500 : Number(params.get("limit")),
          offset: params.get("offset") == null ? 0 : Number(params.get("offset"))
        }));
      }
      var gameMatch = /^games\/([^/]+)$/.exec(route);
      if (gameMatch) {
        var game = byId.get(decodeURIComponent(gameMatch[1]));
        if (!game) return fail(404, "game not found: " + decodeURIComponent(gameMatch[1]));
        return ok({ game: gamePayload(game) });
      }
      if (route === "downloads") {
        if (method === "POST") {
          var ids = Array.isArray(body.ids) ? body.ids : [];
          ids.forEach(function (id) { startDownload(String(id)); });
          return ok({ downloads: downloadList() });
        }
        return ok({ downloads: downloadList() });
      }
      var downloadMatch = /^downloads\/([^/]+)\/(pause|resume|remove)$/.exec(route);
      if (downloadMatch) {
        var downloadId = decodeURIComponent(downloadMatch[1]);
        var action = downloadMatch[2];
        var entry = downloads.get(downloadId);
        if (!entry) return fail(404, "download not found: " + downloadId);
        if (action === "pause") {
          if (entry.controller) entry.controller.abort();
          entry.state = "paused";
        } else if (action === "resume") {
          downloads.delete(downloadId);
          startDownload(downloadId);
        } else {
          if (entry.controller) entry.controller.abort();
          downloads.delete(downloadId);
          library.delete(downloadId);
        }
        return ok({ downloads: downloadList() });
      }
      if (route === "library") return ok({ entries: libraryList(), count: library.size });
      if (route === "library/verify") {
        var results = libraryList().map(function (entry) { return { id: entry.id, status: "ok" }; });
        return ok({ results: results });
      }
      if (route === "sources") {
        if (method === "POST") {
          var name = trim(body.name);
          var url = trim(body.url);
          if (!name || !url) return fail(400, "source name and URL are required");
          sources.push({
            id: "custom:" + url,
            name: name,
            type: "json",
            url: url,
            enabled: true,
            entries: 0,
            last_sync: new Date().toISOString(),
            status: "online",
            custom: true
          });
          return ok({ sources: sources, count: sources.length });
        }
        return ok({ sources: sources, count: sources.length });
      }
      if (route === "sources/test") {
        if (!body.url) return fail(400, "URL is required");
        return Promise.resolve()
          .then(function () { return fetchImpl(body.url, { mode: "cors" }); })
          .then(function (response) {
            if (!response.ok) throw new Error("HTTP " + response.status);
            return response.json();
          })
          .then(function (data) {
            var entries = Array.isArray(data.games) ? data.games.length : Array.isArray(data.entries) ? data.entries.length : 0;
            return ok({ ok: true, entries: entries });
          })
          .catch(function (error) {
            return fail(400, "source test failed: " + String((error && error.message) || error));
          });
      }
      if (route === "config") {
        if (method === "PUT") {
          if (typeof body.download_dir === "string") config.download_dir = body.download_dir || DOWNLOAD_DIR_LABEL;
          if (Number.isFinite(Number(body.jobs))) config.jobs = Math.min(16, Math.max(1, Number(body.jobs)));
          if (typeof body.proxy === "string") config.proxy = body.proxy;
          return ok({ ok: true });
        }
        return ok(config);
      }
      if (route === "play") {
        var playGame = byId.get(String(body.id || ""));
        if (!playGame) return fail(404, "game not found: " + body.id);
        var href = playHref(playGame);
        if (typeof root.open === "function") root.open(href, "_blank", "noopener");
        return ok({ ok: true, url: href });
      }
      if (route === "open") {
        return fail(400, "Revealing the download folder needs the local UI server.");
      }
      if (route === "update") {
        return Promise.resolve()
          .then(function () { return fetchImpl(dataUrl + "?ts=" + Date.now(), { cache: "no-store" }); })
          .then(function (response) { return response.json(); })
          .then(function (fresh) {
            games = Array.isArray(fresh.games) ? fresh.games.slice() : games;
            byId = new Map(games.map(function (item) { return [item.id, item]; }));
            sources = Array.isArray(fresh.sources) ? fresh.sources.slice() : sources;
            return ok(statusPayload());
          })
          .catch(function (error) { return fail(500, "catalog refresh failed: " + String((error && error.message) || error)); });
      }
      return fail(404, "unknown route: " + pathname);
    }

    return {
      handle: handle,
      games: function () { return games; },
      downloads: downloadList,
      library: libraryList
    };
  }

  function install(options) {
    var opts = options || {};
    var dataUrl = opts.dataUrl || "data/catalog.json";
    var nativeFetch = root.fetch ? root.fetch.bind(root) : null;
    if (!nativeFetch) throw new Error("static demo needs fetch()");
    var ready = nativeFetch(dataUrl)
      .then(function (response) {
        if (!response.ok) throw new Error("cannot load " + dataUrl + " (HTTP " + response.status + ")");
        return response.json();
      })
      .then(function (payload) { return createBackend(payload, Object.assign({}, opts, { fetchImpl: nativeFetch })); });

    ready.catch(function (error) {
      console.error("nesearch static demo:", error);
    });

    root.fetch = function (input, init) {
      var url = typeof input === "string" ? input : (input && input.url) || "";
      var parsed = new URL(url, root.location ? root.location.href : "https://example.invalid/");
      if (parsed.pathname.indexOf(API_PREFIX) === -1) {
        return nativeFetch(input, init);
      }
      return ready.then(function (backend) {
        return backend.handle(parsed.pathname, parsed.searchParams, init || {});
      }).then(function (result) {
        return new Response(JSON.stringify(result.body), {
          status: result.status,
          headers: { "Content-Type": "application/json; charset=utf-8" }
        });
      });
    };

    return ready;
  }

  return {
    install: install,
    createBackend: createBackend,
    filterGames: filterGames,
    queryGames: queryGames,
    paginate: paginate,
    relevance: relevance,
    formatBytes: formatBytes
  };
});
