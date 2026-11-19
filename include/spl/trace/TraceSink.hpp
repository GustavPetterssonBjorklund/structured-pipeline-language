#pragma once

#include "TraceEvent.hpp"

namespace spl::trace {

class TraceSink {
public:
    virtual ~TraceSink() = default;
    virtual void emit(const TraceEvent& event) noexcept = 0;
};

} // namespace spl::trace
