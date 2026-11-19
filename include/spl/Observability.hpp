#pragma once

#include <spl/logging/LogSink.hpp>
#include <spl/logging/LogLevel.hpp>
#include <spl/trace/TraceEvent.hpp>
#include <spl/ConsoleHandle.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace spl {

using SubscriptionId = std::uint64_t;
using SerialWriter = std::function<void(std::string_view)>;

void set_log_level(logging::LogLevel level) noexcept;
void set_log_filter(std::string pattern);
SubscriptionId logs_subscribe(std::function<void(const logging::LogRecord&)> callback);
void logs_unsubscribe(SubscriptionId id) noexcept;
void set_serial_interface(SerialWriter writer);
void clear_serial_interface() noexcept;
void set_console_handle(ConsoleHandlePtr handle, bool color = false);
void clear_console_handle() noexcept;
void set_console_color(bool enabled) noexcept;

namespace observability {
void publish_log(const logging::LogRecord& record) noexcept;
void publish_trace(const trace::TraceEvent& event) noexcept;
}

} // namespace spl
