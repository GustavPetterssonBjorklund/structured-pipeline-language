#include <spl/logging/ConsoleFormatter.hpp>

#include <iomanip>
#include <mutex>
#include <set>
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
std::string duration(std::int64_t ns) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << (static_cast<double>(ns) / 1000000.0) << " ms";
    return stream.str();
}

struct ConsoleState {
    std::mutex mutex;
    bool active = false;
    bool saw_trace = false;
    bool pending_completion = false;
    std::string pipeline;
    std::string run_id;
    std::set<std::string> started_nodes;
    std::set<std::string> started_runtimes;
    std::size_t completed_nodes = 0;
};

ConsoleState& console_state() { static ConsoleState state; return state; }

std::string paint(std::string value, const char* color, bool enabled) {
    if (!enabled) return value;
    return std::string(color) + value + "\033[0m";
}

void begin_run(ConsoleState& state, std::string pipeline, std::string run_id) {
    state.active = true;
    state.saw_trace = false;
    state.pending_completion = false;
    state.pipeline = pipeline.empty() ? "Pipeline" : std::move(pipeline);
    state.run_id = std::move(run_id);
    state.started_nodes.clear();
    state.started_runtimes.clear();
    state.completed_nodes = 0;
}

std::string header(const ConsoleState& state) {
    std::string result = "╭─ " + state.pipeline;
    if (!state.run_id.empty()) result += " · " + state.run_id;
    return result + "\n";
}

std::string footer(ConsoleState& state, bool success = true) {
    state.active = false;
    return std::string("╰─ ") + (success ? "✓ pipeline completed\n" : "✗ pipeline failed\n");
}

bool starts_with(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

std::pair<std::string, std::string> parse_run(std::string_view message, std::string_view prefix) {
    std::string rest(message.substr(prefix.size()));
    const auto open = rest.rfind(" (");
    if (open == std::string::npos || rest.back() != ')') return {rest, {}};
    return {rest.substr(0, open), rest.substr(open + 2, rest.size() - open - 3)};
}
}

std::string format_console_log(const LogRecord& record, bool color) {
    auto& state = console_state();
    std::lock_guard lock(state.mutex);
    const auto level = std::string(log_level_name(record.level));
    if (record.module == "engine.graph" && starts_with(record.message, "pipeline started: ")) {
        const auto [pipeline, run_id] = parse_run(record.message, "pipeline started: ");
        if (!state.active) { begin_run(state, pipeline, run_id); return header(state); }
        return {};
    }
    if (record.module == "engine.graph" && starts_with(record.message, "node started: ") && state.active) {
        const auto node = record.message.substr(std::string("node started: ").size());
        if (state.started_nodes.insert(node).second) return "│  ├─ " + node + "\n";
        return {};
    }
    if (starts_with(record.message, "invoking ") && state.active) {
        const auto target = record.message.substr(std::string("invoking ").size());
        if (state.started_runtimes.insert(target).second) return "│  │  ↳ " + target + "\n";
        return {};
    }
    if (record.module == "engine.graph" && starts_with(record.message, "pipeline completed: ") && state.active) {
        state.pending_completion = true;
        return {};
    }
    if (state.active) {
        const auto line = "│  " + level + " [" + record.module + "] " + record.message + "\n";
        return paint(line, level_color(record.level), color);
    }
    const auto timestamp = record.timestamp.empty() ? std::string{} : "[" + record.timestamp + "] ";
    const auto prefix = timestamp + "[" + level + "]  [" + record.module + "] ";
    if (!color) return prefix + record.message + '\n';
    return std::string(level_color(record.level)) + prefix + record.message + "\033[0m\n";
}

std::string format_console_trace(const trace::TraceEvent& event, bool color) {
    auto& state = console_state();
    std::lock_guard lock(state.mutex);
    if (event.kind == trace::TraceEventKind::PipelineStarted) {
        begin_run(state, event.pipeline_name, event.run_id);
        state.saw_trace = true;
        return paint(header(state), "\033[36m", color);
    }
    if (!state.active) begin_run(state, event.pipeline_name, event.run_id);
    state.saw_trace = true;
    if (event.kind == trace::TraceEventKind::NodeStarted) {
        if (!state.started_nodes.insert(event.node_instance_id).second) return {};
        return "│  ├─ " + event.node_instance_id + "\n";
    }
    if (event.kind == trace::TraceEventKind::RuntimeInvoked) {
        if (!state.started_runtimes.insert(event.invocation_id).second) return {};
        return "│  │  ↳ " + event.runtime_id + "\n";
    }
    if (event.kind == trace::TraceEventKind::NodeCompleted || event.kind == trace::TraceEventKind::NodeFailed) {
        ++state.completed_nodes;
        const bool success = event.kind == trace::TraceEventKind::NodeCompleted;
        std::string line = "│  └─ ";
        line += success ? "✓ " : "✗ ";
        line += event.node_instance_id;
        if (event.duration_ns > 0) line += " · " + duration(event.duration_ns);
        if (!event.error.empty()) line += " · " + event.error;
        line += '\n';
        if (state.pending_completion && state.completed_nodes >= state.started_nodes.size()) {
            line += footer(state);
        }
        return paint(std::move(line), success ? "\033[32m" : "\033[31m", color);
    }
    if (event.kind == trace::TraceEventKind::PipelineCompleted || event.kind == trace::TraceEventKind::PipelineFailed) {
        return footer(state, event.kind == trace::TraceEventKind::PipelineCompleted);
    }
    if (event.kind == trace::TraceEventKind::RuntimeCompleted) {
        return {};
    }
    return {};
}

} // namespace spl::logging
