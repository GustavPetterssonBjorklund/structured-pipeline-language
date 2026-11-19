#pragma once

#include "Type.hpp"

#include <cstdint>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace spl {

class Value {
public:
    using Storage = std::variant<std::int64_t, double, bool, std::string>;

    explicit Value(std::int64_t value) : data_(value) {}
    explicit Value(double value) : data_(value) {}
    explicit Value(bool value) : data_(value) {}
    explicit Value(std::string value) : data_(std::move(value)) {}

    Type type() const noexcept {
        switch (data_.index()) {
            case 0: return Type::Int64;
            case 1: return Type::Float64;
            case 2: return Type::Bool;
            case 3: return Type::String;
            default: return Type::String; // std::variant always has a value here.
        }
    }

    bool is(Type expected) const noexcept { return type() == expected; }

    template <typename T>
    const T& get() const {
        return std::get<T>(data_);
    }

    std::string to_string() const {
        return std::visit([](const auto& value) -> std::string {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, std::string>) {
                return value;
            } else if constexpr (std::is_same_v<T, bool>) {
                return value ? "true" : "false";
            } else if constexpr (std::is_same_v<T, double>) {
                std::ostringstream stream;
                stream << std::setprecision(17) << value;
                return stream.str();
            } else {
                return std::to_string(value);
            }
        }, data_);
    }

private:
    Storage data_;
};

using Values = std::map<std::string, Value>;

inline bool same_type(const Value& left, Type right) noexcept {
    return left.type() == right;
}

} // namespace spl
