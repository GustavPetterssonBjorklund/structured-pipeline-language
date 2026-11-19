#include <spl/runtime/JavaScriptAdapter.hpp>
#include <spl/logging/Logger.hpp>

#include <fstream>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <atomic>

namespace spl::runtime {
namespace {

std::string json_escape(const std::string& value) {
    std::string result = "\"";
    for (const unsigned char character : value) {
        switch (character) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (character < 0x20) {
                    std::ostringstream escaped;
                    escaped << "\\u" << std::hex << static_cast<int>(character);
                    result += escaped.str();
                } else result += static_cast<char>(character);
        }
    }
    result += '"';
    return result;
}

std::string json_value(const Value& value) {
    if (value.is(Type::Int64)) return json_escape(std::to_string(value.get<std::int64_t>()));
    if (value.is(Type::Float64)) return value.to_string();
    if (value.is(Type::Bool)) return value.get<bool>() ? "true" : "false";
    return json_escape(value.get<std::string>());
}

std::string quote_shell(const std::string& value) {
#ifdef _WIN32
    std::string result = "\"";
    for (const char character : value) {
        if (character == '"') result += "\\\"";
        else result += character;
    }
    result += '"';
    return result;
#else
    std::string result = "'";
    for (const char character : value) {
        if (character == '\'') result += "'\\''";
        else result += character;
    }
    result += '\'';
    return result;
#endif
}

class JsonReader {
public:
    explicit JsonReader(std::string text) : text_(std::move(text)) {}

    std::map<std::string, std::map<std::string, std::string>> outputs() {
        skip(); expect('{'); skip();
        const auto key = string();
        skip(); expect(':'); skip();
        if (key == "version") {
            scalar(); skip(); expect(','); skip();
            const auto outputs_key = string();
            if (outputs_key != "outputs") throw std::runtime_error("JavaScript worker response has no outputs");
            skip(); expect(':'); skip();
        } else if (key != "outputs") throw std::runtime_error("JavaScript worker response has no outputs");
        expect('{'); skip();
        std::map<std::string, std::map<std::string, std::string>> result;
        if (peek() == '}') { advance(); return result; }
        while (true) {
            const auto name = string(); skip(); expect(':'); skip(); expect('{');
            std::map<std::string, std::string> fields;
            skip();
            while (peek() != '}') {
                const auto field = string(); skip(); expect(':'); skip();
                fields[field] = scalar(); skip();
                if (peek() == ',') { advance(); skip(); } else break;
            }
            expect('}'); result.emplace(name, std::move(fields)); skip();
            if (peek() == ',') { advance(); skip(); } else break;
        }
        expect('}'); return result;
    }

private:
    char peek() const { return position_ < text_.size() ? text_[position_] : '\0'; }
    void advance() { if (position_ < text_.size()) ++position_; }
    void skip() { while (peek() == ' ' || peek() == '\n' || peek() == '\r' || peek() == '\t') advance(); }
    void expect(char expected) { if (peek() != expected) throw std::runtime_error("invalid JavaScript worker response"); advance(); }
    std::string string() {
        expect('"'); std::string result;
        while (peek() != '"') {
            if (peek() == '\0') throw std::runtime_error("unterminated string in JavaScript worker response");
            if (peek() == '\\') {
                advance(); const char escaped = peek(); advance();
                if (escaped == 'n') result += '\n'; else if (escaped == 'r') result += '\r';
                else if (escaped == 't') result += '\t'; else result += escaped;
            } else { result += peek(); advance(); }
        }
        advance(); return result;
    }
    std::string scalar() {
        if (peek() == '"') return string();
        const auto start = position_;
        while (peek() != ',' && peek() != '}' && peek() != '\0' && peek() != ' ' && peek() != '\n') advance();
        return text_.substr(start, position_ - start);
    }
    std::string text_;
    std::size_t position_ = 0;
};

Value decode_value(const std::map<std::string, std::string>& fields, const std::string& name, const ExecutionContext& context) {
    const auto type = fields.find("type"); const auto raw = fields.find("value");
    if (type == fields.end() || raw == fields.end()) throw std::runtime_error("JavaScript output is malformed: " + name);
    const auto expected = std::find_if(context.outputs.begin(), context.outputs.end(), [&](const PortSpec& port) { return port.name == name; });
    if (expected == context.outputs.end()) throw std::runtime_error("JavaScript output is unexpected: " + name);
    if (type->second != type_name(expected->type)) throw std::runtime_error("JavaScript output '" + name + "' has the wrong SPL type");
    try {
        if (expected->type == Type::Int64) return Value{static_cast<std::int64_t>(std::stoll(raw->second))};
        if (expected->type == Type::Float64) return Value{std::stod(raw->second)};
        if (expected->type == Type::Bool) {
            if (raw->second == "true") return Value{true};
            if (raw->second == "false") return Value{false};
            throw std::runtime_error("invalid Bool output");
        }
        return Value{raw->second};
    } catch (...) { throw std::runtime_error("JavaScript output '" + name + "' has an invalid value"); }
}

std::atomic<std::uint64_t> request_counter{0};

} // namespace

JavaScriptAdapter::JavaScriptAdapter(std::string node_executable, std::string worker_path)
    : node_executable_(std::move(node_executable)), worker_path_(std::move(worker_path)) {}

std::string JavaScriptAdapter::id() const { return "javascript"; }

NodeResult JavaScriptAdapter::invoke(const Implementation& implementation, const Values& inputs, const ExecutionContext& context) {
    const auto logger = logging::Logger::module("runtime.javascript");
    logger.debug("invoking {}", implementation.target);
    const auto base = std::filesystem::temp_directory_path() / ("spl-javascript-" + std::to_string(++request_counter));
    const auto request = base.string() + ".request.json";
    const auto response = base.string() + ".response.json";
    const auto error = base.string() + ".stderr";
    try {
        std::ofstream stream(request);
        if (!stream) throw std::runtime_error("cannot create JavaScript runtime request");
        stream << "{\"target\":" << json_escape(implementation.target) << ",\"inputs\":{";
        bool first = true;
        for (const auto& [name, value] : inputs) {
            if (!first) stream << ',';
            first = false;
            stream << json_escape(name) << ":{\"type\":" << json_escape(std::string(type_name(value.type()))) << ",\"value\":" << json_value(value) << '}';
        }
        stream << "},\"outputs\":{";
        first = true;
        for (const auto& port : context.outputs) {
            if (!first) stream << ',';
            first = false;
            stream << json_escape(port.name) << ':' << json_escape(std::string(type_name(port.type)));
        }
        stream << "}}\n";
        stream.close();
        const auto command = quote_shell(node_executable_) + " " + quote_shell(worker_path_) + " --request " + quote_shell(request) + " --response " + quote_shell(response) + " 2>" + quote_shell(error);
        if (std::system(command.c_str()) != 0) {
            std::ifstream errors(error); std::stringstream text; text << errors.rdbuf();
            throw std::runtime_error("JavaScript node failed" + (text.str().empty() ? std::string{} : ": " + text.str()));
        }
        std::ifstream result_stream(response);
        std::stringstream result_text; result_text << result_stream.rdbuf();
        const auto parsed = JsonReader(result_text.str()).outputs();
        Values outputs;
        for (const auto& port : context.outputs) {
            const auto found = parsed.find(port.name);
            if (found == parsed.end()) throw std::runtime_error("JavaScript output is missing '" + port.name + "'");
            outputs.emplace(port.name, decode_value(found->second, port.name, context));
        }
        for (const auto& [name, fields] : parsed) {
            (void)fields;
            if (std::none_of(context.outputs.begin(), context.outputs.end(), [&](const PortSpec& port) { return port.name == name; })) {
                throw std::runtime_error("JavaScript output is unexpected: " + name);
            }
        }
        std::filesystem::remove(request); std::filesystem::remove(response); std::filesystem::remove(error);
        return outputs;
    } catch (...) {
        std::filesystem::remove(request); std::filesystem::remove(response); std::filesystem::remove(error);
        throw;
    }
}

} // namespace spl::runtime
