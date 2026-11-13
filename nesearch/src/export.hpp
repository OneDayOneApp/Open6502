#pragma once

#include <nlohmann/json.hpp>

#include "catalog.hpp"
#include "config.hpp"

namespace nes {

// Shapes shared by the live UI API (`nesearch ui`) and the static export
// consumed by the GitHub Pages demo. Keep both call sites on these helpers so
// the browser shim never drifts from the server.
nlohmann::json game_export_json(const Game& game);
nlohmann::json source_export_json(const Config& config, const CatalogService& catalog);

// `{schema, generator, generated_at, fetched_at, totals, sources, games}`.
nlohmann::json catalog_export_json(const Config& config, const CatalogService& catalog);

}  // namespace nes
