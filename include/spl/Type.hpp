#pragma once

#include <string_view>

namespace spl {

enum class Type {
    Int64,
    Float64,
    Bool,
    String
};

constexpr std::string_view type_name(Type type) noexcept {
    switch (type) {
        case Type::Int64:   return "Int64";
        case Type::Float64: return "Float64";
        case Type::Bool:    return "Bool";
        case Type::String:  return "String";
    }
    return "Unknown";
}

} // namespace spl
