#pragma once

#include "Graph.hpp"

#include <filesystem>
#include <string_view>

namespace spl {

Graph parse_pipeline(std::string_view source, std::filesystem::path source_path = {});
Graph parse_pipeline_file(const std::filesystem::path& path);

} // namespace spl
