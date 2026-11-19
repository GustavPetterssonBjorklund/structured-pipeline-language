#pragma once

#include "Lexer.hpp"

namespace spl::parser {

class SyntaxParser {
public:
    explicit SyntaxParser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}
    ProgramSyntax parse();

private:
    const Token& current() const;
    bool accept(TokenKind kind);
    Token expect(TokenKind kind, const char* description);
    std::string expect_identifier(const char* description);
    std::string expect_type();
    NodeSyntax parse_node();
    PipelineSyntax parse_pipeline();
    PortSyntax parse_port(bool input);
    InstanceSyntax parse_instance();
    OutputDeclarationSyntax parse_output();
    BindingSyntax parse_binding();

    std::vector<Token> tokens_;
    std::size_t index_ = 0;
};

} // namespace spl::parser
