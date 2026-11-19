#include "support/TestAssertions.hpp"

#include <spl/Graph.hpp>

#include <cstdint>

int main() {
    using namespace spl;
    using namespace spl::test;

    Graph graph;
    graph.add_error_definition(ErrorDefinition{
        "ValidationError", {{"message", Type::String}, {"field", Type::String}}});
    graph.add_definition(NodeDefinition{
        "ValidatePositive",
        {{"value", PortDirection::Input, Type::Int64}, {"result", PortDirection::Output, Type::Int64}},
        [](const Values& inputs) -> NodeResult {
            const auto value = inputs.at("value").get<std::int64_t>();
            if (value < 0) {
                return ErrorValue{
                    ErrorDefinition{"ValidationError", {{"message", Type::String}, {"field", Type::String}}},
                    Values{{"message", Value{std::string{"must be non-negative"}}},
                           {"field", Value{std::string{"value"}}}}};
            }
            return Values{{"result", Value{value}}};
        },
        {"ValidationError"},
        {}});
    graph.add_input("value", Type::Int64);
    graph.add_instance(NodeInstance{"validation", "ValidatePositive", {{"value", Binding::pipeline_input("value")}}});
    graph.add_output("result", Binding::node_output("validation", "result"));

    const auto result = graph.execute(Values{{"value", Value{std::int64_t{-1}}}});
    require(!result.succeeded(), "domain error was not returned");
    require(result.error->definition_id() == "ValidationError", "wrong domain error type");
    require(result.trace.back().typed_error.has_value(), "typed error was absent from trace");

    require_graph_error([] {
        Graph invalid;
        invalid.add_error_definition(ErrorDefinition{"Duplicate", {{"field", Type::String}, {"field", Type::String}}});
    }, "duplicate error fields were accepted");
}
