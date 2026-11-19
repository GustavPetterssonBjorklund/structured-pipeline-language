#pragma once

#include <spl/logging/LogSink.hpp>
#include <spl/trace/TraceEvent.hpp>

#include <string>

namespace spl::logging {

std::string format_console_log(const LogRecord& record, bool color = false);
std::string format_console_trace(const trace::TraceEvent& event, bool color = false);

} // namespace spl::logging
