#include <spl/logging/LogSink.hpp>
#include <spl/logging/Logger.hpp>
#include <spl/logging/ConsoleSink.hpp>
#include <spl/logging/ConsoleFormatter.hpp>

#include <iostream>
#include <mutex>

namespace spl::logging {

class ConsoleSink final : public LogSink {
public:
    void emit(const LogRecord& record) noexcept override {
        try {
            static std::mutex mutex;
            std::lock_guard lock(mutex);
            std::cerr << format_console_log(record, false);
        } catch (...) {}
    }
    bool is_fallback_console() const noexcept override { return true; }
};

std::shared_ptr<LogSink> make_console_sink() { return std::make_shared<ConsoleSink>(); }

} // namespace spl::logging
