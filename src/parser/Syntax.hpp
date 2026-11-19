#pragma once

#include <string>
#include <vector>

namespace spl::parser {

struct SourceLocation {
    std::size_t offset = 0;
    std::size_t line = 1;
    std::size_t column = 1;
};

struct BindingSyntax {
    enum class Kind { Integer, Float, Bool, String, Reference };
    Kind kind;
    std::string value;
    std::string port;
    SourceLocation location;
};

struct PortSyntax {
    bool input;
    std::string name;
    std::string type;
    SourceLocation location;
};

struct NodeSyntax {
    std::string id;
    std::vector<PortSyntax> ports;
    std::string runtime;
    std::string target;
    SourceLocation location;
};

struct InputSyntax {
    std::string name;
    std::string type;
    SourceLocation location;
};

struct InstanceSyntax {
    std::string id;
    std::string definition;
    std::vector<std::pair<std::string, BindingSyntax>> arguments;
    SourceLocation location;
};

struct OutputSyntax {
    std::string name;
    std::string type;
    BindingSyntax binding;
    SourceLocation location;
};

struct OutputDeclarationSyntax {
    std::string name;
    std::string type;
    SourceLocation location;
};

struct PipelineSyntax {
    std::string id;
    std::vector<InputSyntax> inputs;
    std::vector<InstanceSyntax> instances;
    std::vector<OutputDeclarationSyntax> output_declarations;
    std::vector<OutputSyntax> outputs;
    SourceLocation location;
};

struct ProgramSyntax {
    std::vector<NodeSyntax> nodes;
    PipelineSyntax pipeline;
};

} // namespace spl::parser
