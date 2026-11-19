#include "Lexer.hpp"

#include <spl/Graph.hpp>

#include <cctype>
#include <stdexcept>

namespace spl::parser {
namespace {

[[noreturn]] void fail(const SourceLocation& location, const std::string& message) {
    throw GraphError("line " + std::to_string(location.line) + ", column " + std::to_string(location.column) + ": " + message);
}

TokenKind keyword(std::string_view text) {
    if (text == "node") return TokenKind::Node;
    if (text == "pipeline") return TokenKind::Pipeline;
    if (text == "input") return TokenKind::Input;
    if (text == "output") return TokenKind::Output;
    if (text == "use") return TokenKind::Use;
    if (text == "impl") return TokenKind::Impl;
    if (text == "true") return TokenKind::True;
    if (text == "false") return TokenKind::False;
    if (text == "Int64" || text == "Float64" || text == "Bool" || text == "String") return TokenKind::TypeName;
    return TokenKind::Identifier;
}

} // namespace

std::vector<Token> Lexer::tokenize() const {
    std::vector<Token> tokens;
    SourceLocation location;
    std::size_t index = 0;
    auto advance = [&](char character) {
        ++index;
        if (character == '\n') { ++location.line; location.column = 1; }
        else ++location.column;
    };
    while (index < source_.size()) {
        const char character = source_[index];
        if (std::isspace(static_cast<unsigned char>(character))) { advance(character); continue; }
        if (character == '/' && index + 1 < source_.size() && source_[index + 1] == '/') {
            while (index < source_.size() && source_[index] != '\n') advance(source_[index]);
            continue;
        }
        const auto start = location;
        if (std::isalpha(static_cast<unsigned char>(character)) || character == '_') {
            const auto begin = index;
            while (index < source_.size() && (std::isalnum(static_cast<unsigned char>(source_[index])) || source_[index] == '_')) advance(source_[index]);
            const std::string text(source_.substr(begin, index - begin));
            tokens.push_back({keyword(text), text, start});
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(character)) || (character == '-' && index + 1 < source_.size() && std::isdigit(static_cast<unsigned char>(source_[index + 1])))) {
            const auto begin = index;
            if (source_[index] == '-') advance(source_[index]);
            while (index < source_.size() && std::isdigit(static_cast<unsigned char>(source_[index]))) advance(source_[index]);
            auto kind = TokenKind::Integer;
            if (index < source_.size() && source_[index] == '.') {
                kind = TokenKind::Float; advance(source_[index]);
                while (index < source_.size() && std::isdigit(static_cast<unsigned char>(source_[index]))) advance(source_[index]);
            }
            tokens.push_back({kind, std::string(source_.substr(begin, index - begin)), start});
            continue;
        }
        if (character == '"') {
            advance(character);
            std::string value;
            bool closed = false;
            while (index < source_.size()) {
                const char current = source_[index];
                if (current == '"') { advance(current); closed = true; break; }
                if (current == '\\') {
                    advance(current);
                    if (index >= source_.size()) break;
                    const char escaped = source_[index];
                    if (escaped == 'n') value += '\n';
                    else if (escaped == 't') value += '\t';
                    else if (escaped == 'r') value += '\r';
                    else if (escaped == '"' || escaped == '\\') value += escaped;
                    else fail(location, "unsupported string escape");
                    advance(escaped);
                } else { value += current; advance(current); }
            }
            if (!closed) fail(start, "unterminated string literal");
            tokens.push_back({TokenKind::String, std::move(value), start});
            continue;
        }
        TokenKind kind;
        switch (character) {
            case '{': kind = TokenKind::LBrace; break; case '}': kind = TokenKind::RBrace; break;
            case '(': kind = TokenKind::LParen; break; case ')': kind = TokenKind::RParen; break;
            case ':': kind = TokenKind::Colon; break; case ',': kind = TokenKind::Comma; break;
            case '.': kind = TokenKind::Dot; break; case '=': kind = TokenKind::Equals; break;
            default: fail(start, "unexpected character");
        }
        advance(character);
        tokens.push_back({kind, std::string(1, character), start});
    }
    tokens.push_back({TokenKind::End, {}, location});
    return tokens;
}

} // namespace spl::parser
