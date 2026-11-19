#pragma once

#include <spl/Graph.hpp>

#include <cstdint>

namespace spl::test {

inline NodeDefinition add_definition() {
    return NodeDefinition{
        "Add",
        {{"a", PortDirection::Input, Type::Int64},
         {"b", PortDirection::Input, Type::Int64},
         {"sum", PortDirection::Output, Type::Int64}},
        [](const Values& inputs) {
            return Values{{"sum", Value{inputs.at("a").get<std::int64_t>() + inputs.at("b").get<std::int64_t>()}}};
        },
        {},
        {}};
}

inline NodeDefinition multiply_definition() {
    return NodeDefinition{
        "Multiply",
        {{"a", PortDirection::Input, Type::Int64},
         {"b", PortDirection::Input, Type::Int64},
         {"product", PortDirection::Output, Type::Int64}},
        [](const Values& inputs) {
            return Values{{"product", Value{inputs.at("a").get<std::int64_t>() * inputs.at("b").get<std::int64_t>()}}};
        },
        {},
        {}};
}

} // namespace spl::test
