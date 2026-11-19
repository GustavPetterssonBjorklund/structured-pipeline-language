#pragma once

#include "LogSink.hpp"
#include <memory>

namespace spl::logging { std::shared_ptr<LogSink> make_console_sink(); }
