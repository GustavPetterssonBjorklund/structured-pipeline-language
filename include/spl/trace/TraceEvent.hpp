#pragma once

#include <spl/Error.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace spl::trace {

enum class TraceEventKind {
    PipelineStarted, PipelineCompleted, PipelineFailed,
    NodeStarted, NodeCompleted, NodeFailed,
    RuntimeInvoked, RuntimeCompleted
};

std::string_view trace_event_kind_name(TraceEventKind kind) noexcept;

struct TraceEvent {
    TraceEventKind kind = TraceEventKind::NodeStarted;
    std::string run_id;
    std::string pipeline_name;
    std::string node_definition_id;
    std::string node_instance_id;
    std::string invocation_id;
    std::string runtime_id;
    std::string timestamp;
    std::int64_t duration_ns = 0;
    Values inputs;
    Values outputs;
    std::map<std::string, std::string> input_values;
    std::map<std::string, std::string> output_values;
    std::string error;
    std::optional<ErrorValue> typed_error;
    std::map<std::string, std::string> metadata;
};

} // namespace spl::trace
