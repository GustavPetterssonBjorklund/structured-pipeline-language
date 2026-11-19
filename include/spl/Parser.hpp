#pragma once

#include "Graph.hpp"

#include <filesystem>

namespace spl {

Graph parse_pipeline_file(const std::filesystem::path& path);

} // namespace spl
