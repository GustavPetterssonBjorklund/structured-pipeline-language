#pragma once

#include "Syntax.hpp"
#include <spl/Graph.hpp>

#include <filesystem>

namespace spl::parser {

Graph compile(const ProgramSyntax& program, const std::filesystem::path& source_path);

} // namespace spl::parser
