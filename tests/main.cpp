#include <spl/Graph.hpp>

#include <cassert>
#include <cstdint>
#include <iostream>

using namespace spl;

static NodeDefinition add_definition() {
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

static NodeDefinition multiply_definition() {
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

int main() {
    Value score{95.5};
    assert(score.is(Type::Float64));
    assert(score.to_string() == "95.5");

    Graph graph;
    graph.add_definition(add_definition());
    graph.add_definition(multiply_definition());
    graph.add_input("x", Type::Int64);
    graph.add_input("y", Type::Int64);
    graph.add_instance(NodeInstance{
        "sum", "Add", {{"a", Binding::pipeline_input("x")}, {"b", Binding::pipeline_input("y")}}});
    graph.add_instance(NodeInstance{
        "product", "Multiply", {{"a", Binding::node_output("sum", "sum")}, {"b", Binding::literal_value(Value{std::int64_t{2}})}}});
    graph.add_output("result", Binding::node_output("product", "product"));

    const auto result = graph.execute(Values{{"x", Value{std::int64_t{3}}}, {"y", Value{std::int64_t{4}}}});
    assert(result.outputs.at("result").get<std::int64_t>() == 14);
    assert(result.trace.size() == 2);
    assert(result.trace.at(0).node_id == "sum");
    assert(result.trace.at(1).status == "succeeded");

    bool rejected = false;
    try {
        Graph invalid;
        invalid.add_definition(add_definition());
        invalid.add_input("x", Type::Int64);
        invalid.add_instance(NodeInstance{
            "bad", "Add", {{"a", Binding::pipeline_input("x")}, {"b", Binding::literal_value(Value{true})}}});
        invalid.validate();
    } catch (const GraphError&) {
        rejected = true;
    }
    assert(rejected);

    Graph error_graph;
    error_graph.add_error_definition(ErrorDefinition{
        "ValidationError",
        {{"message", Type::String}, {"field", Type::String}}});
    error_graph.add_definition(NodeDefinition{
        "ValidatePositive",
        {{"value", PortDirection::Input, Type::Int64},
         {"result", PortDirection::Output, Type::Int64}},
        [](const Values& inputs) -> NodeResult {
            const auto value = inputs.at("value").get<std::int64_t>();
            if (value < 0) {
                return ErrorValue{
                    ErrorDefinition{
                        "ValidationError",
                        {{"message", Type::String}, {"field", Type::String}}},
                    Values{{"message", Value{std::string{"must be non-negative"}}},
                           {"field", Value{std::string{"value"}}}}};
            }
            return Values{{"result", Value{value}}};
        },
        {"ValidationError"},
        {}});
    error_graph.add_input("value", Type::Int64);
    error_graph.add_instance(NodeInstance{
        "validation", "ValidatePositive", {{"value", Binding::pipeline_input("value")}}});
    error_graph.add_output("result", Binding::node_output("validation", "result"));

    const auto error_result = error_graph.execute(Values{{"value", Value{std::int64_t{-1}}}});
    assert(!error_result.succeeded());
    assert(error_result.error->definition_id() == "ValidationError");
    assert(error_result.trace.back().typed_error.has_value());
    assert(error_result.trace.back().error.find("ValidationError") != std::string::npos);

    std::cout << "SPL core tests passed\n";
}
