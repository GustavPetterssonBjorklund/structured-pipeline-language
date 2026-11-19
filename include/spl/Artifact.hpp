#pragma once

#include "Graph.hpp"

#include <filesystem>

namespace spl {

void write_pipeline_artifact(const Graph& graph, const std::filesystem::path& path);
Graph read_pipeline_artifact(const std::filesystem::path& path);

} // namespace spl
