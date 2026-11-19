#include "support/TestAssertions.hpp"
#include "support/TestFixtures.hpp"

#include <cstdint>

int main() {
    using namespace spl;
    using namespace spl::test;

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
    require(result.outputs.at("result").get<std::int64_t>() == 14, "graph result was incorrect");
    require(result.trace.size() == 2, "graph trace length was incorrect");
    require(result.trace.at(0).node_id == "sum", "graph execution was not deterministic");
    require(result.trace.at(1).status == "succeeded", "node did not succeed");

    Graph invalid;
    invalid.add_definition(add_definition());
    invalid.add_input("x", Type::Int64);
    invalid.add_instance(NodeInstance{
        "bad", "Add", {{"a", Binding::pipeline_input("x")}, {"b", Binding::literal_value(Value{true})}}});
    require_graph_error([&] { invalid.validate(); }, "invalid graph type was accepted");

    Graph output_type;
    output_type.add_definition(add_definition());
    output_type.add_input("x", Type::Int64);
    output_type.add_input("y", Type::Int64);
    output_type.add_instance(NodeInstance{
        "sum", "Add", {{"a", Binding::pipeline_input("x")}, {"b", Binding::pipeline_input("y")}}});
    output_type.add_output("result", Type::String, Binding::node_output("sum", "sum"));
    require_graph_error([&] { output_type.validate(); }, "invalid pipeline output type was accepted");
}
