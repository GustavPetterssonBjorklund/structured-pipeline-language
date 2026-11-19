#pragma once

#include "Syntax.hpp"

#include <string_view>
#include <vector>

namespace spl::parser {

enum class TokenKind {
    Identifier, Integer, Float, String, True, False,
    Node, Pipeline, Input, Output, Use, Impl, TypeName,
    LBrace, RBrace, LParen, RParen, Colon, Comma, Dot, Equals, End
};

struct Token {
    TokenKind kind;
    std::string text;
    SourceLocation location;
};

class Lexer {
public:
    explicit Lexer(std::string_view source) : source_(source) {}
    std::vector<Token> tokenize() const;

private:
    std::string_view source_;
};

} // namespace spl::parser
