#pragma once

#include "TraceSink.hpp"

#include <mutex>
#include <vector>

namespace spl::trace {

class TraceCollector final : public TraceSink {
public:
    void emit(const TraceEvent& event) noexcept override {
        try { std::lock_guard lock(mutex_); events_.push_back(event); } catch (...) {}
    }

    std::vector<TraceEvent> events() const {
        std::lock_guard lock(mutex_);
        return events_;
    }

private:
    mutable std::mutex mutex_;
    std::vector<TraceEvent> events_;
};

} // namespace spl::trace
