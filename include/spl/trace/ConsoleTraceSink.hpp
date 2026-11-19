#pragma once

#include "TraceSink.hpp"

#include <memory>

namespace spl::trace { std::shared_ptr<TraceSink> make_console_trace_sink(); }
