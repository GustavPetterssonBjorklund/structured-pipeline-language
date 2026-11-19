#include "SyntaxParser.hpp"

#include <spl/Graph.hpp>

namespace spl::parser {
namespace {

[[noreturn]] void fail(const Token& token, const std::string& message) {
    throw GraphError("line " + std::to_string(token.location.line) + ", column " + std::to_string(token.location.column) + ": " + message);
}

} // namespace

const Token& SyntaxParser::current() const { return tokens_.at(index_); }

bool SyntaxParser::accept(TokenKind kind) {
    if (current().kind != kind) return false;
    ++index_;
    return true;
}

Token SyntaxParser::expect(TokenKind kind, const char* description) {
    if (current().kind != kind) fail(current(), std::string("expected ") + description);
    return tokens_[index_++];
}

std::string SyntaxParser::expect_identifier(const char* description) {
    return expect(TokenKind::Identifier, description).text;
}

std::string SyntaxParser::expect_type() { return expect(TokenKind::TypeName, "a primitive type").text; }

ProgramSyntax SyntaxParser::parse() {
    ProgramSyntax program;
    while (current().kind != TokenKind::End) {
        if (accept(TokenKind::Node)) program.nodes.push_back(parse_node());
        else if (accept(TokenKind::Pipeline)) {
            program.pipelines.push_back(parse_pipeline());
        } else fail(current(), "expected node or pipeline declaration");
    }
    if (program.pipelines.empty()) fail(current(), "no pipeline declaration found");
    return program;
}

NodeSyntax SyntaxParser::parse_node() {
    NodeSyntax node;
    node.id = expect_identifier("node name");
    expect(TokenKind::LBrace, "'{' after node name");
    while (!accept(TokenKind::RBrace)) {
        if (accept(TokenKind::Input)) node.ports.push_back(parse_port(true));
        else if (accept(TokenKind::Output)) node.ports.push_back(parse_port(false));
        else if (accept(TokenKind::Impl)) {
            node.runtime = expect_identifier("runtime name");
            node.target = expect(TokenKind::String, "implementation target").text;
        } else fail(current(), "expected input, output, or impl in node");
    }
    if (node.runtime.empty()) fail(current(), "node has no implementation");
    return node;
}

PortSyntax SyntaxParser::parse_port(bool input) {
    const auto token = current();
    return PortSyntax{input, expect_identifier("port name"), (expect(TokenKind::Colon, "':' after port name"), expect_type()), token.location};
}

PipelineSyntax SyntaxParser::parse_pipeline() {
    PipelineSyntax pipeline;
    pipeline.id = expect_identifier("pipeline name");
    expect(TokenKind::LBrace, "'{' after pipeline name");
    while (!accept(TokenKind::RBrace)) {
        if (accept(TokenKind::Input)) {
            const auto location = current().location;
            pipeline.inputs.push_back(InputSyntax{expect_identifier("pipeline input name"), (expect(TokenKind::Colon, "':' after input name"), expect_type()), location});
        } else if (accept(TokenKind::Output)) {
            pipeline.output_declarations.push_back(parse_output());
        } else if (accept(TokenKind::Use)) {
            pipeline.instances.push_back(parse_instance());
        } else if (current().kind == TokenKind::Identifier) {
            const auto name = expect_identifier("output name");
            const auto location = current().location;
            expect(TokenKind::Equals, "'=' in output assignment");
            std::string type;
            for (const auto& declaration : pipeline.output_declarations) {
                if (declaration.name == name) { type = declaration.type; break; }
            }
            if (type.empty()) fail(current(), "output assignment has no matching output declaration: " + name);
            pipeline.outputs.push_back(OutputSyntax{name, type, parse_binding(), location});
        } else fail(current(), "expected pipeline input, output, use, or assignment");
    }
    for (const auto& declaration : pipeline.output_declarations) {
        bool assigned = false;
        for (const auto& output : pipeline.outputs) if (output.name == declaration.name) assigned = true;
        if (!assigned) fail(current(), "pipeline output has no assignment: " + declaration.name);
    }
    return pipeline;
}

OutputDeclarationSyntax SyntaxParser::parse_output() {
    const auto location = current().location;
    const auto name = expect_identifier("pipeline output name");
    const auto type = (expect(TokenKind::Colon, "':' after output name"), expect_type());
    return OutputDeclarationSyntax{name, type, location};
}

InstanceSyntax SyntaxParser::parse_instance() {
    InstanceSyntax instance;
    instance.id = expect_identifier("node instance name");
    expect(TokenKind::Colon, "':' before node definition");
    instance.definition = expect_identifier("node definition name");
    expect(TokenKind::LParen, "'(' before node arguments");
    if (!accept(TokenKind::RParen)) {
        do {
            const auto name = expect_identifier("node input name");
            expect(TokenKind::Equals, "'=' in node argument");
            instance.arguments.emplace_back(name, parse_binding());
        } while (accept(TokenKind::Comma));
        expect(TokenKind::RParen, "')' after node arguments");
    }
    return instance;
}

BindingSyntax SyntaxParser::parse_binding() {
    const auto token = current();
    if (accept(TokenKind::Integer)) return {BindingSyntax::Kind::Integer, token.text, {}, token.location};
    if (accept(TokenKind::Float)) return {BindingSyntax::Kind::Float, token.text, {}, token.location};
    if (accept(TokenKind::True)) return {BindingSyntax::Kind::Bool, "true", {}, token.location};
    if (accept(TokenKind::False)) return {BindingSyntax::Kind::Bool, "false", {}, token.location};
    if (accept(TokenKind::String)) return {BindingSyntax::Kind::String, token.text, {}, token.location};
    const auto name = expect_identifier("binding");
    if (accept(TokenKind::Dot)) return {BindingSyntax::Kind::Reference, name, expect_identifier("output port name"), token.location};
    return {BindingSyntax::Kind::Reference, name, {}, token.location};
}

} // namespace spl::parser
