#pragma once

#include "TraceSink.hpp"

#include <memory>
#include <string>

namespace spl::trace { std::shared_ptr<TraceSink> make_json_trace_sink(const std::string& path); }
