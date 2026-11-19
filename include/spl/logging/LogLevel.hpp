#pragma once

#include <string_view>

namespace spl::logging {

enum class LogLevel { Trace = 0, Debug, Info, Warn, Error, Fatal };

std::string_view log_level_name(LogLevel level) noexcept;
bool parse_log_level(std::string_view value, LogLevel& level) noexcept;

} // namespace spl::logging
