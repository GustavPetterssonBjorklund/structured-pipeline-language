#include <spl/trace/ConsoleTraceSink.hpp>

#include <iostream>
#include <mutex>

namespace spl::trace {
namespace {
class ConsoleTraceSink final : public TraceSink {
public:
    void emit(const TraceEvent& event) noexcept override {
        try {
            static std::mutex mutex;
            std::lock_guard lock(mutex);
            std::cerr << "[TRACE] [" << trace_event_kind_name(event.kind) << "]";
            if (!event.node_instance_id.empty()) std::cerr << " [" << event.node_instance_id << "]";
            if (event.duration_ns > 0) std::cerr << " (" << event.duration_ns / 1000000.0 << " ms)";
            if (!event.error.empty()) std::cerr << " " << event.error;
            std::cerr << '\n';
        } catch (...) {}
    }
};
}
std::shared_ptr<TraceSink> make_console_trace_sink() { return std::make_shared<ConsoleTraceSink>(); }
} // namespace spl::trace
