#include <spl/logging/ConsoleFormatter.hpp>

#include <iomanip>
#include <sstream>

namespace spl::logging {
namespace {
const char* level_color(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "\033[90m";
        case LogLevel::Debug: return "\033[36m";
        case LogLevel::Info: return "\033[32m";
        case LogLevel::Warn: return "\033[33m";
        case LogLevel::Error: return "\033[31m";
        case LogLevel::Fatal: return "\033[1;31m";
    }
    return "";
}
const char* event_color(trace::TraceEventKind kind) {
    switch (kind) {
        case trace::TraceEventKind::NodeFailed:
        case trace::TraceEventKind::PipelineFailed: return "\033[31m";
        case trace::TraceEventKind::NodeCompleted:
        case trace::TraceEventKind::PipelineCompleted: return "\033[32m";
        case trace::TraceEventKind::RuntimeInvoked: return "\033[36m";
        default: return "\033[90m";
    }
}
std::string duration(std::int64_t ns) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << (static_cast<double>(ns) / 1000000.0) << " ms";
    return stream.str();
}
}

std::string format_console_log(const LogRecord& record, bool color) {
    const auto timestamp = record.timestamp.empty() ? std::string{} : "[" + record.timestamp + "] ";
    const auto prefix = timestamp + "[" + std::string(log_level_name(record.level)) + "]  [" + record.module + "] ";
    if (!color) return prefix + record.message + '\n';
    return std::string(level_color(record.level)) + prefix + record.message + "\033[0m\n";
}

std::string format_console_trace(const trace::TraceEvent& event, bool color) {
    std::string label;
    switch (event.kind) {
        case trace::TraceEventKind::NodeStarted: label = "START"; break;
        case trace::TraceEventKind::NodeCompleted: label = "OK"; break;
        case trace::TraceEventKind::NodeFailed: label = "FAILED"; break;
        case trace::TraceEventKind::RuntimeInvoked: label = "RUNTIME"; break;
        case trace::TraceEventKind::RuntimeCompleted: label = "RUNTIME OK"; break;
        case trace::TraceEventKind::PipelineStarted: label = "PIPELINE START"; break;
        case trace::TraceEventKind::PipelineCompleted: label = "PIPELINE OK"; break;
        case trace::TraceEventKind::PipelineFailed: label = "PIPELINE FAILED"; break;
    }
    std::string result = "[TRACE] [" + label + "]";
    if (!event.node_instance_id.empty()) result += " [" + event.node_instance_id + "]";
    if (!event.runtime_id.empty()) result += " [" + event.runtime_id + "]";
    if (event.duration_ns > 0) result += " (" + duration(event.duration_ns) + ")";
    if (!event.error.empty()) result += " " + event.error;
    result += '\n';
    if (color) return std::string(event_color(event.kind)) + result + "\033[0m";
    return result;
}

} // namespace spl::logging
