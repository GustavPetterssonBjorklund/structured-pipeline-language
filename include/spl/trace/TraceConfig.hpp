#pragma once

#include "TraceSink.hpp"
#include <spl/logging/LogLevel.hpp>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace spl::trace {

enum class ObservabilityMode { Off, Summary, Trace, Debug };

struct ValueContext {
    std::string node_id;
    std::string port;
    bool output = false;
};

using ValueRedactor = std::function<std::string(const Value&, const ValueContext&)>;

struct ObservabilityConfig {
    ObservabilityMode mode = ObservabilityMode::Off;
    logging::LogLevel log_level = logging::LogLevel::Warn;
    bool capture_inputs = false;
    bool capture_outputs = false;
    bool include_timestamps = false;
    std::size_t max_value_length = 4096;
    std::size_t max_events = 100000;
    ValueRedactor redactor;
};

struct ExecutionOptions {
    ObservabilityConfig observability;
    std::vector<std::shared_ptr<TraceSink>> trace_sinks;
    std::shared_ptr<class TraceCollector> collector;
};

} // namespace spl::trace
