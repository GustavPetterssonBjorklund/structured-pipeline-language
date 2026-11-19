#include <spl/runtime/PythonAdapter.hpp>

#include <cstdio>
#include <regex>
#include <stdexcept>

namespace spl::runtime {
namespace {

std::string shell_quote(const std::string& value) {
    std::string quoted = "'";
    for (const char character : value) {
        if (character == '\'') quoted += "'\\''";
        else quoted += character;
    }
    return quoted + "'";
}

std::string json_escape(const std::string& value) {
    std::string result;
    for (const auto character : value) {
        if (character == '\\') result += "\\\\";
        else if (character == '"') result += "\\\"";
        else if (character == '\n') result += "\\n";
        else if (character == '\t') result += "\\t";
        else result += character;
    }
    return result;
}

std::string json_value(const Value& value) {
    if (value.is(Type::String)) return "\"" + json_escape(value.get<std::string>()) + "\"";
    if (value.is(Type::Bool)) return value.get<bool>() ? "true" : "false";
    return value.to_string();
}

Value parse_json_value(const std::string& type, std::string value) {
    if (type == "Int64") return Value{static_cast<std::int64_t>(std::stoll(value))};
    if (type == "Float64") return Value{std::stod(value)};
    if (type == "Bool") return Value{value == "true"};
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') value = value.substr(1, value.size() - 2);
    return Value{std::move(value)};
}

} // namespace

PythonAdapter::PythonAdapter(std::string worker_path) : worker_path_(std::move(worker_path)) {}

std::string PythonAdapter::id() const { return "python"; }

NodeResult PythonAdapter::invoke(
    const Implementation& implementation,
    const Values& inputs,
    const ExecutionContext&) {
    std::string request = "{\"version\":1,\"inputs\":{";
    bool first = true;
    for (const auto& [name, value] : inputs) {
        if (!first) request += ',';
        first = false;
        request += "\"" + json_escape(name) + "\":{";
        request += "\"type\":\"" + std::string(type_name(value.type())) + "\",\"value\":" + json_value(value) + "}";
    }
    request += "}}";
    const auto command = "python3 " + shell_quote(worker_path_) + " --target " + shell_quote(implementation.target)
        + " --input-json " + shell_quote(request) + " 2>&1";

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) throw std::runtime_error("failed to start Python runtime");
    char buffer[4096];
    std::string output;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) output += buffer;
    const int status = pclose(pipe);
    if (status != 0) throw std::runtime_error("Python runtime failed: " + output);

    Values values;
    const std::regex entry(R"REGEX("([A-Za-z_][A-Za-z0-9_]*)":\{"type":"(Int64|Float64|Bool|String)","value":(true|false|-?[0-9]+(?:\.[0-9]+)?|"(?:[^"\\]|\\.)*")\})REGEX");
    for (std::sregex_iterator it(output.begin(), output.end(), entry), end; it != end; ++it) {
        values.emplace((*it)[1].str(), parse_json_value((*it)[2].str(), (*it)[3].str()));
    }
    if (values.empty() && output.find("\"outputs\":{}") == std::string::npos) {
        throw std::runtime_error("Python runtime returned malformed response: " + output);
    }
    return values;
}

} // namespace spl::runtime
