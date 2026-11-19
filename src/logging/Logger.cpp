#include <spl/logging/Logger.hpp>
#include <spl/Observability.hpp>

#include <chrono>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <regex>
#include <sstream>

namespace spl::logging {
namespace {

struct State {
    std::mutex mutex;
    LogLevel level = LogLevel::Warn;
    std::string filter;
    bool timestamps = false;
    bool fallback_console_enabled = true;
    std::vector<std::shared_ptr<LogSink>> sinks;
};

State& state() { static State value; return value; }

bool matches(std::string_view module, std::string_view pattern) {
    if (pattern.empty() || pattern == "*") return true;
    std::string expression;
    for (const char character : pattern) {
        if (character == '*') expression += ".*";
        else if (std::string(".^$|()[]{}+?\\").find(character) != std::string::npos) {
            expression += '\\'; expression += character;
        } else expression += character;
    }
    try { return std::regex_match(std::string(module), std::regex(expression)); }
    catch (...) { return false; }
}

std::string timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
#if defined(_WIN32)
    gmtime_s(&local, &time);
#else
    gmtime_r(&time, &local);
#endif
    std::ostringstream result;
    result << std::put_time(&local, "%Y-%m-%dT%H:%M:%SZ");
    return result.str();
}

} // namespace

std::string_view log_level_name(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warn: return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
    }
    return "UNKNOWN";
}

bool parse_log_level(std::string_view value, LogLevel& level) noexcept {
    for (const auto candidate : {LogLevel::Trace, LogLevel::Debug, LogLevel::Info, LogLevel::Warn, LogLevel::Error, LogLevel::Fatal}) {
        if (value == log_level_name(candidate)) { level = candidate; return true; }
        std::string lower(value);
        for (char& character : lower) character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        std::string candidate_name(log_level_name(candidate));
        for (char& character : candidate_name) character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        if (lower == candidate_name) { level = candidate; return true; }
    }
    return false;
}

Logger Logger::module(std::string name) { return Logger(std::move(name)); }

void Logger::set_level(LogLevel level) noexcept { std::lock_guard lock(state().mutex); state().level = level; }
LogLevel Logger::level() noexcept { std::lock_guard lock(state().mutex); return state().level; }
void Logger::set_filter(std::string pattern) { std::lock_guard lock(state().mutex); state().filter = std::move(pattern); }
void Logger::clear_filter() noexcept { std::lock_guard lock(state().mutex); state().filter.clear(); }
void Logger::set_timestamps(bool enabled) noexcept { std::lock_guard lock(state().mutex); state().timestamps = enabled; }
void Logger::add_sink(std::shared_ptr<LogSink> sink) { if (sink) { std::lock_guard lock(state().mutex); state().sinks.push_back(std::move(sink)); } }
void Logger::clear_sinks() noexcept { std::lock_guard lock(state().mutex); state().sinks.clear(); }
void Logger::set_fallback_console_enabled(bool enabled) noexcept { std::lock_guard lock(state().mutex); state().fallback_console_enabled = enabled; }

bool Logger::enabled(LogLevel level) const noexcept {
    std::lock_guard lock(state().mutex);
    return static_cast<int>(level) >= static_cast<int>(state().level) && matches(module_, state().filter);
}

std::string Logger::format_message(std::string_view format, const std::vector<std::string>& args) {
    std::string result;
    std::size_t argument = 0;
    for (std::size_t index = 0; index < format.size(); ++index) {
        if (format[index] == '{' && index + 1 < format.size() && format[index + 1] == '}') {
            result += argument < args.size() ? args[argument++] : "{}"; ++index;
        } else result += format[index];
    }
    return result;
}

void Logger::emit(LogLevel level, std::string message) const noexcept {
    try {
        std::vector<std::shared_ptr<LogSink>> sinks;
        LogRecord record{level, module_, std::move(message), {}, {}};
        {
            std::lock_guard lock(state().mutex);
            if (static_cast<int>(level) < static_cast<int>(state().level) || !matches(module_, state().filter)) return;
            if (state().timestamps) record.timestamp = timestamp();
            sinks = state().sinks;
        }
        bool fallback_enabled;
        { std::lock_guard lock(state().mutex); fallback_enabled = state().fallback_console_enabled; }
        for (const auto& sink : sinks) if (fallback_enabled || !sink->is_fallback_console()) sink->emit(record);
        observability::publish_log(record);
    } catch (...) {}
}

} // namespace spl::logging
