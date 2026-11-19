#pragma once

#include "LogLevel.hpp"

#include <map>
#include <string>

namespace spl::logging {

struct LogRecord {
    LogLevel level;
    std::string module;
    std::string message;
    std::string timestamp;
    std::map<std::string, std::string> metadata;
};

class LogSink {
public:
    virtual ~LogSink() = default;
    virtual void emit(const LogRecord& record) noexcept = 0;
    virtual bool is_fallback_console() const noexcept { return false; }
};

} // namespace spl::logging
