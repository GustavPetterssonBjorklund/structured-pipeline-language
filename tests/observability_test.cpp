#include "support/TestAssertions.hpp"
#include "support/TestFixtures.hpp"

#include <spl/logging/Logger.hpp>
#include <spl/logging/ConsoleFormatter.hpp>
#include <spl/Observability.hpp>
#include <spl/trace/TraceCollector.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

int main() {
    using namespace spl;
    using namespace spl::test;

    Graph graph;
    graph.add_definition(add_definition());
    graph.add_input("x", Type::Int64);
    graph.add_input("y", Type::Int64);
    graph.add_instance(NodeInstance{"sum", "Add", {{"a", Binding::pipeline_input("x")}, {"b", Binding::pipeline_input("y")}}});
    graph.add_output("result", Binding::node_output("sum", "sum"));

    auto collector = std::make_shared<trace::TraceCollector>();
    trace::ExecutionOptions options;
    options.observability.mode = trace::ObservabilityMode::Debug;
    options.observability.max_value_length = 2;
    options.collector = collector;
    const auto result = graph.execute(Values{{"x", Value{std::int64_t{3}}}, {"y", Value{std::int64_t{4}}}}, nullptr, options);
    require(result.outputs.at("result").get<std::int64_t>() == 7, "observability graph result was incorrect");
    require(!collector->events().empty(), "trace collector did not receive events");
    require(collector->events().front().kind == trace::TraceEventKind::PipelineStarted, "pipeline start event was missing");
    require(collector->events().back().kind == trace::TraceEventKind::PipelineCompleted, "pipeline completion event was missing");
    require(result.statistics.pipeline_duration_ns >= 0, "pipeline duration was negative");
    require(result.statistics.nodes.size() == 1, "node statistics were missing");
    require(result.statistics.nodes.front().execution_duration_ns >= 0, "node duration was negative");

    std::size_t log_count = 0;
    const auto subscription = logs_subscribe([&](const logging::LogRecord& record) {
        require(record.module == "test", "subscription received the wrong module");
        ++log_count;
    });
    logging::Logger::set_level(logging::LogLevel::Info);
    logging::Logger::module("test").info("hello {}", 1);
    logs_unsubscribe(subscription);
    logging::Logger::module("test").info("ignored");
    require(log_count == 1, "log subscription lifecycle was incorrect");

    std::vector<std::string> serial_lines;
    set_serial_interface([&](std::string_view line) { serial_lines.emplace_back(line); });
    logging::Logger::module("test").info("serial");
    const auto serial_result = graph.execute(Values{{"x", Value{std::int64_t{1}}}, {"y", Value{std::int64_t{2}}}}, nullptr, options);
    (void)serial_result;
    clear_serial_interface();
    require(serial_lines.size() >= 2, "serial interface did not receive log and trace records");

    trace::TraceEvent start;
    start.kind = trace::TraceEventKind::PipelineStarted;
    start.pipeline_name = "Calculation";
    start.run_id = "test-run";
    const auto header = logging::format_console_trace(start);
    require(header.find("╭─ Calculation · test-run") != std::string::npos, "console tree header was missing");
    trace::TraceEvent done;
    done.kind = trace::TraceEventKind::NodeCompleted;
    done.node_instance_id = "sum";
    done.duration_ns = 800000;
    const auto node = logging::format_console_trace(done);
    require(node.find("✓ sum") != std::string::npos, "console tree node completion was missing");
}
