#pragma once

#include "Syntax.hpp"
#include <spl/Graph.hpp>

#include <filesystem>
#include <string_view>

namespace spl::parser {

Graph compile(const ProgramSyntax& program, const std::filesystem::path& source_path,
              std::string_view pipeline_name = {});

} // namespace spl::parser
