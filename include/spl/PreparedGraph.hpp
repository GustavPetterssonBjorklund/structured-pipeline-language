#pragma once

#include "Graph.hpp"

namespace spl {

class PreparedGraph {
public:
    explicit PreparedGraph(Graph graph);

    const std::map<std::string, Type>& input_types() const noexcept;
    std::map<std::string, Type> output_types() const;
    ExecutionResult execute(
        const Values& pipeline_inputs,
        const runtime::RuntimeRegistry* runtime_registry = nullptr,
        bool collect_trace = false) const;

private:
    struct PreparedBinding {
        Binding::Kind kind = Binding::Kind::Literal;
        Value literal{std::int64_t{0}};
        std::size_t input_slot = 0;
        std::size_t node_slot = 0;
        std::string port_name;
    };

    struct PreparedNode {
        std::string id;
        const NodeDefinition* definition = nullptr;
        std::vector<std::pair<std::string, PreparedBinding>> inputs;
    };

    struct PreparedOutput {
        std::string name;
        std::optional<Type> type;
        PreparedBinding binding;
    };

    static PreparedBinding prepare_binding(
        const Graph& graph,
        const Binding& binding,
        const std::map<std::string, std::size_t>& input_slots,
        const std::map<std::string, std::size_t>& node_slots);

    Graph graph_;
    std::vector<std::string> order_;
    std::map<std::string, std::size_t> input_slots_;
    std::vector<PreparedNode> nodes_;
    std::vector<PreparedOutput> outputs_;
};

} // namespace spl
