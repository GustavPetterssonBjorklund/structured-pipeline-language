#include <spl/trace/JsonTraceSink.hpp>

#include <fstream>
#include <mutex>

namespace spl::trace {
namespace {
std::string json(const std::string& value) {
    std::string result = "\"";
    for (const char character : value) {
        if (character == '\\' || character == '\"') result += '\\';
        if (character == '\n') result += "\\n";
        else if (character == '\r') result += "\\r";
        else if (character == '\t') result += "\\t";
        else result += character;
    }
    return result + '"';
}
std::string strings(const std::map<std::string, std::string>& values) {
    std::string result = "{"; bool first = true;
    for (const auto& [name, value] : values) {
        if (!first) result += ',';
        first = false;
        result += json(name) + ':' + json(value);
    }
    return result + '}';
}
class JsonTraceSink final : public TraceSink {
public:
    explicit JsonTraceSink(const std::string& path) : stream_(path, std::ios::out | std::ios::app) {}
    void emit(const TraceEvent& event) noexcept override {
        try {
            std::lock_guard lock(mutex_);
            if (!stream_) return;
            stream_ << "{\"event_kind\":" << json(std::string(trace_event_kind_name(event.kind)))
                    << ",\"run_id\":" << json(event.run_id)
                    << ",\"pipeline_name\":" << json(event.pipeline_name)
                    << ",\"node_instance_id\":" << json(event.node_instance_id)
                    << ",\"node_definition_id\":" << json(event.node_definition_id)
                    << ",\"invocation_id\":" << json(event.invocation_id)
                    << ",\"runtime_id\":" << json(event.runtime_id)
                    << ",\"timestamp\":" << json(event.timestamp)
                    << ",\"duration_ns\":" << event.duration_ns
                    << ",\"inputs\":" << strings(event.input_values)
                    << ",\"outputs\":" << strings(event.output_values)
                    << ",\"error\":" << json(event.error) << "}\n";
            stream_.flush();
        } catch (...) {}
    }
private:
    std::ofstream stream_;
    std::mutex mutex_;
};
}
std::shared_ptr<TraceSink> make_json_trace_sink(const std::string& path) { return std::make_shared<JsonTraceSink>(path); }
} // namespace spl::trace
