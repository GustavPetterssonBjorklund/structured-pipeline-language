#include <spl/trace/TraceEvent.hpp>

namespace spl::trace {
std::string_view trace_event_kind_name(TraceEventKind kind) noexcept {
    switch (kind) {
        case TraceEventKind::PipelineStarted: return "pipeline_started";
        case TraceEventKind::PipelineCompleted: return "pipeline_completed";
        case TraceEventKind::PipelineFailed: return "pipeline_failed";
        case TraceEventKind::NodeStarted: return "node_started";
        case TraceEventKind::NodeCompleted: return "node_completed";
        case TraceEventKind::NodeFailed: return "node_failed";
        case TraceEventKind::RuntimeInvoked: return "runtime_invoked";
        case TraceEventKind::RuntimeCompleted: return "runtime_completed";
    }
    return "unknown";
}
} // namespace spl::trace
