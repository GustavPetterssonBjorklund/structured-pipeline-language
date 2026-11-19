#include <spl/Observability.hpp>
#include <spl/logging/Logger.hpp>
#include <spl/logging/ConsoleFormatter.hpp>

#include <atomic>
#include <map>
#include <mutex>
#include <vector>

namespace spl {
namespace {
std::mutex mutex;
std::map<SubscriptionId, std::function<void(const logging::LogRecord&)>> subscribers;
SerialWriter serial_writer;
ConsoleHandlePtr console_handle;
bool console_color = false;
std::atomic<SubscriptionId> next_id{0};

}

void set_log_level(logging::LogLevel level) noexcept { logging::Logger::set_level(level); }
void set_log_filter(std::string pattern) { logging::Logger::set_filter(std::move(pattern)); }

SubscriptionId logs_subscribe(std::function<void(const logging::LogRecord&)> callback) {
    if (!callback) return 0;
    const auto id = ++next_id;
    std::lock_guard lock(mutex);
    subscribers.emplace(id, std::move(callback));
    return id;
}

void logs_unsubscribe(SubscriptionId id) noexcept {
    std::lock_guard lock(mutex);
    subscribers.erase(id);
}

void set_serial_interface(SerialWriter writer) {
    bool active = static_cast<bool>(writer);
    {
        std::lock_guard lock(mutex);
        serial_writer = std::move(writer);
        console_handle.reset();
    }
    logging::Logger::set_fallback_console_enabled(!active);
}

void clear_serial_interface() noexcept {
    {
        std::lock_guard lock(mutex);
        serial_writer = {};
        console_handle.reset();
    }
    logging::Logger::set_fallback_console_enabled(true);
}

void set_console_handle(ConsoleHandlePtr handle, bool color) {
    {
        std::lock_guard lock(mutex);
        console_handle = handle;
        console_color = color;
        serial_writer = handle ? [handle](std::string_view text) { handle->write(text); handle->flush(); } : SerialWriter{};
    }
    logging::Logger::set_fallback_console_enabled(!handle);
}

void clear_console_handle() noexcept { clear_serial_interface(); }
void set_console_color(bool enabled) noexcept { std::lock_guard lock(mutex); console_color = enabled; }

namespace observability {
void publish_log(const logging::LogRecord& record) noexcept {
    try {
        std::vector<std::function<void(const logging::LogRecord&)>> callbacks;
        SerialWriter writer;
        {
            std::lock_guard lock(mutex);
            for (const auto& [id, callback] : subscribers) { (void)id; callbacks.push_back(callback); }
            writer = serial_writer;
        }
        for (const auto& callback : callbacks) { try { callback(record); } catch (...) {} }
        bool color;
        { std::lock_guard lock(mutex); color = console_color; }
        if (writer) { try { writer(logging::format_console_log(record, color)); } catch (...) {} }
    } catch (...) {}
}

void publish_trace(const trace::TraceEvent& event) noexcept {
    try {
        SerialWriter writer;
        { std::lock_guard lock(mutex); writer = serial_writer; }
        bool color;
        { std::lock_guard lock(mutex); color = console_color; }
        if (writer) { try { writer(logging::format_console_trace(event, color)); } catch (...) {} }
    } catch (...) {}
}
} // namespace observability
} // namespace spl
