#include <spl/Parser.hpp>

#include <fstream>
#include <regex>
#include <sstream>

namespace spl {
namespace {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) throw GraphError("cannot open SPL file: " + path.string());
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::vector<std::string> split_arguments(const std::string& text) {
    std::vector<std::string> result;
    std::stringstream stream(text);
    std::string item;
    while (std::getline(stream, item, ',')) result.push_back(trim(item));
    return result;
}

Binding parse_binding(const std::string& text) {
    const auto value = trim(text);
    if (std::regex_match(value, std::regex("-?[0-9]+"))) {
        return Binding::literal_value(Value{static_cast<std::int64_t>(std::stoll(value))});
    }
    if (value == "true" || value == "false") {
        return Binding::literal_value(Value{value == "true"});
    }
    if (std::regex_match(value, std::regex(R"(-?(?:[0-9]+\.[0-9]*|[0-9]*\.[0-9]+))"))) {
        return Binding::literal_value(Value{std::stod(value)});
    }
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        std::string decoded;
        for (std::size_t i = 1; i + 1 < value.size(); ++i) {
            if (value[i] == '\\' && i + 1 < value.size() - 1) {
                const auto escaped = value[++i];
                decoded += escaped == 'n' ? '\n' : escaped == 't' ? '\t' : escaped;
            } else decoded += value[i];
        }
        return Binding::literal_value(Value{std::move(decoded)});
    }
    const auto dot = value.find('.');
    if (dot != std::string::npos) {
        return Binding::node_output(value.substr(0, dot), value.substr(dot + 1));
    }
    return Binding::pipeline_input(value);
}

} // namespace

Graph parse_pipeline_file(const std::filesystem::path& path) {
    const auto source = read_file(path);
    Graph graph;

    const std::regex node_pattern(R"(node\s+([A-Za-z_][A-Za-z0-9_]*)\s*\{([\s\S]*?)\})");
    const std::regex port_pattern(R"((input|output)\s+([A-Za-z_][A-Za-z0-9_]*)\s*:\s*(Int64|Float64|Bool|String))");
    const std::regex impl_pattern(R"REGEX(impl\s+([A-Za-z_][A-Za-z0-9_]*)\s+"([^"]+)")REGEX");

    for (std::sregex_iterator it(source.begin(), source.end(), node_pattern), end; it != end; ++it) {
        NodeDefinition definition;
        definition.id = (*it)[1].str();
        const auto body = (*it)[2].str();
        for (std::sregex_iterator port(body.begin(), body.end(), port_pattern), port_end; port != port_end; ++port) {
            const auto direction = (*port)[1].str() == "input" ? PortDirection::Input : PortDirection::Output;
            const auto type_name = (*port)[3].str();
            const auto type = type_name == "Int64" ? Type::Int64
                : type_name == "Float64" ? Type::Float64
                : type_name == "Bool" ? Type::Bool : Type::String;
            definition.ports.push_back(Port{(*port)[2].str(), direction, type});
        }
        std::smatch implementation;
        if (!std::regex_search(body, implementation, impl_pattern)) {
            throw GraphError("node has no implementation: " + definition.id);
        }
        definition.runtime_implementation = runtime::Implementation{
            implementation[1].str(),
            (path.parent_path() / implementation[2].str()).lexically_normal().string()};
        graph.add_definition(std::move(definition));
    }

    const std::regex pipeline_pattern(R"(pipeline\s+[A-Za-z_][A-Za-z0-9_]*\s*\{([\s\S]*?)\})");
    std::smatch pipeline;
    if (!std::regex_search(source, pipeline, pipeline_pattern)) throw GraphError("no pipeline declaration found");
    const auto body = pipeline[1].str();

    const std::regex input_pattern(R"(input\s+([A-Za-z_][A-Za-z0-9_]*)\s*:\s*(Int64|Float64|Bool|String))");
    for (std::sregex_iterator it(body.begin(), body.end(), input_pattern), end; it != end; ++it) {
        const auto type = (*it)[2].str() == "Int64" ? Type::Int64
            : (*it)[2].str() == "Float64" ? Type::Float64
            : (*it)[2].str() == "Bool" ? Type::Bool : Type::String;
        graph.add_input((*it)[1].str(), type);
    }

    const std::regex use_pattern(R"(use\s+([A-Za-z_][A-Za-z0-9_]*)\s*:\s*([A-Za-z_][A-Za-z0-9_]*)\s*\(([^)]*)\))");
    for (std::sregex_iterator it(body.begin(), body.end(), use_pattern), end; it != end; ++it) {
        NodeInstance instance;
        instance.id = (*it)[1].str();
        instance.definition_id = (*it)[2].str();
        for (const auto& argument : split_arguments((*it)[3].str())) {
            const auto equals = argument.find('=');
            if (equals == std::string::npos) throw GraphError("expected named node argument: " + argument);
            instance.inputs.emplace(trim(argument.substr(0, equals)), parse_binding(argument.substr(equals + 1)));
        }
        graph.add_instance(std::move(instance));
    }

    const std::regex output_pattern(R"(output\s+([A-Za-z_][A-Za-z0-9_]*)\s*:\s*(Int64|Float64|Bool|String))");
    bool has_output = false;
    for (std::sregex_iterator it(body.begin(), body.end(), output_pattern), end; it != end; ++it) {
        has_output = true;
        const auto name = (*it)[1].str();
        const auto type_name = (*it)[2].str();
        const auto type = type_name == "Int64" ? Type::Int64
            : type_name == "Float64" ? Type::Float64
            : type_name == "Bool" ? Type::Bool : Type::String;
        const std::regex assignment("\\b" + name + R"(\s*=\s*([^\s]+))");
        std::smatch match;
        if (!std::regex_search(body, match, assignment)) {
            throw GraphError("pipeline output has no assignment: " + name);
        }
        graph.add_output(name, type, parse_binding(match[1].str()));
    }
    if (!has_output) throw GraphError("pipeline has no output declaration");
    graph.validate();
    return graph;
}

} // namespace spl
