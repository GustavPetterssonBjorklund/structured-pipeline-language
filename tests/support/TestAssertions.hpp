#pragma once

#include <spl/Graph.hpp>

#include <stdexcept>
#include <string>

namespace spl::test {

inline void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename Function>
void require_graph_error(Function&& function, const std::string& message) {
    try {
        function();
    } catch (const GraphError&) {
        return;
    }
    throw std::runtime_error(message);
}

} // namespace spl::test
