#pragma once

#include "LogSink.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <sstream>

namespace spl::logging {

class Logger {
public:
    static Logger module(std::string name);

    static void set_level(LogLevel level) noexcept;
    static LogLevel level() noexcept;
    static void set_filter(std::string pattern);
    static void clear_filter() noexcept;
    static void set_timestamps(bool enabled) noexcept;
    static void add_sink(std::shared_ptr<LogSink> sink);
    static void clear_sinks() noexcept;
    static void set_fallback_console_enabled(bool enabled) noexcept;

    bool enabled(LogLevel level) const noexcept;

    template <typename... Args>
    void trace(std::string_view format, Args&&... args) const { log(LogLevel::Trace, format, std::forward<Args>(args)...); }
    template <typename... Args>
    void debug(std::string_view format, Args&&... args) const { log(LogLevel::Debug, format, std::forward<Args>(args)...); }
    template <typename... Args>
    void info(std::string_view format, Args&&... args) const { log(LogLevel::Info, format, std::forward<Args>(args)...); }
    template <typename... Args>
    void warn(std::string_view format, Args&&... args) const { log(LogLevel::Warn, format, std::forward<Args>(args)...); }
    template <typename... Args>
    void error(std::string_view format, Args&&... args) const { log(LogLevel::Error, format, std::forward<Args>(args)...); }
    template <typename... Args>
    void fatal(std::string_view format, Args&&... args) const { log(LogLevel::Fatal, format, std::forward<Args>(args)...); }

private:
    explicit Logger(std::string name) : module_(std::move(name)) {}

    template <typename T>
    static std::string argument_string(T&& value) { std::ostringstream stream; stream << value; return stream.str(); }
    static std::string argument_string(const std::string& value) { return value; }
    static std::string argument_string(std::string_view value) { return std::string(value); }
    static std::string argument_string(const char* value) { return value ? value : "(null)"; }

    template <typename... Args>
    void log(LogLevel level, std::string_view format, Args&&... args) const {
        if (!enabled(level)) return;
        std::vector<std::string> values{argument_string(std::forward<Args>(args))...};
        emit(level, format_message(format, values));
    }

    void emit(LogLevel level, std::string message) const noexcept;
    static std::string format_message(std::string_view format, const std::vector<std::string>& args);
    std::string module_;
};

} // namespace spl::logging
