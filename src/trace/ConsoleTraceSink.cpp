#include <spl/trace/ConsoleTraceSink.hpp>
#include <spl/logging/ConsoleFormatter.hpp>

#include <iostream>

namespace spl::trace {
namespace {
class ConsoleTraceSink final : public TraceSink {
public:
    void emit(const TraceEvent& event) noexcept override {
        try {
            std::cerr << logging::format_console_trace(event, false);
        } catch (...) {}
    }
};
}
std::shared_ptr<TraceSink> make_console_trace_sink() { return std::make_shared<ConsoleTraceSink>(); }
} // namespace spl::trace
