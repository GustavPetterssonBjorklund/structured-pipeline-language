#pragma once

#include <cstddef>
#include <memory>
#include <string_view>

namespace spl {

class ConsoleHandle {
public:
    virtual ~ConsoleHandle() = default;
    virtual void write(std::string_view text) noexcept = 0;
    virtual void flush() noexcept {}
};

using ConsoleHandlePtr = std::shared_ptr<ConsoleHandle>;

} // namespace spl
