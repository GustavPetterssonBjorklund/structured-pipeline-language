#include <spl/Parser.hpp>

#include "parser/Compiler.hpp"
#include "parser/Lexer.hpp"
#include "parser/SyntaxParser.hpp"

#include <fstream>
#include <sstream>

namespace spl {

Graph parse_pipeline(std::string_view source, std::filesystem::path source_path,
                     std::string_view pipeline_name) {
    parser::Lexer lexer(source);
    parser::SyntaxParser syntax_parser(lexer.tokenize());
    return parser::compile(syntax_parser.parse(), source_path, pipeline_name);
}

Graph parse_pipeline_file(const std::filesystem::path& path, std::string_view pipeline_name) {
    std::ifstream file(path);
    if (!file) throw GraphError("cannot open SPL file: " + path.string());
    std::ostringstream contents;
    contents << file.rdbuf();
    return parse_pipeline(contents.str(), path, pipeline_name);
}

} // namespace spl
