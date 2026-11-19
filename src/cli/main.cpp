#include <spl/Artifact.hpp>
#include <spl/Parser.hpp>
#include <spl/PreparedGraph.hpp>
#include <spl/runtime/PythonAdapter.hpp>
#include <spl/runtime/JavaScriptAdapter.hpp>
#include <spl/logging/Logger.hpp>
#include <spl/logging/ConsoleSink.hpp>
#include <spl/trace/ConsoleTraceSink.hpp>
#include <spl/trace/JsonTraceSink.hpp>

#include <iostream>
#include <memory>
#include <string>

namespace {
bool option_value(int& index, int argc, char** argv, const std::string& name, std::string& value) {
    if (std::string(argv[index]) != name || index + 1 >= argc) return false;
    value = argv[++index];
    return true;
}
}

int main(int argc, char** argv) {
    try {
        std::filesystem::path pipeline = "examples/pipeline.spl";
        spl::trace::ExecutionOptions options;
        std::string value;
        for (int index = 1; index < argc; ++index) {
            if (argv[index][0] != '-') { pipeline = argv[index]; continue; }
            if (option_value(index, argc, argv, "--observability", value)) {
                if (value == "off") options.observability.mode = spl::trace::ObservabilityMode::Off;
                else if (value == "summary") options.observability.mode = spl::trace::ObservabilityMode::Summary;
                else if (value == "trace") options.observability.mode = spl::trace::ObservabilityMode::Trace;
                else if (value == "debug") { options.observability.mode = spl::trace::ObservabilityMode::Debug; options.observability.capture_inputs = true; options.observability.capture_outputs = true; }
                else throw std::runtime_error("unknown observability mode: " + value);
            } else if (option_value(index, argc, argv, "--log-level", value)) {
                if (!spl::logging::parse_log_level(value, options.observability.log_level)) throw std::runtime_error("unknown log level: " + value);
            } else if (option_value(index, argc, argv, "--log-filter", value)) {
                spl::logging::Logger::set_filter(value);
            } else if (option_value(index, argc, argv, "--trace-output", value)) {
                options.trace_sinks.push_back(spl::trace::make_json_trace_sink(value));
            } else throw std::runtime_error("unknown option: " + std::string(argv[index]));
        }
        spl::logging::Logger::set_level(options.observability.log_level);
        spl::logging::Logger::add_sink(spl::logging::make_console_sink());
        if (options.observability.mode == spl::trace::ObservabilityMode::Trace || options.observability.mode == spl::trace::ObservabilityMode::Debug) {
            options.trace_sinks.push_back(spl::trace::make_console_trace_sink());
        }
        spl::Values inputs{{"x", spl::Value{std::int64_t{3}}}, {"y", spl::Value{std::int64_t{4}}}};
        spl::runtime::RuntimeRegistry runtimes;
        runtimes.register_adapter(std::make_unique<spl::runtime::PythonAdapter>());
        runtimes.register_adapter(std::make_unique<spl::runtime::JavaScriptAdapter>());
        const auto graph = pipeline.extension() == ".splb"
            ? spl::read_pipeline_artifact(pipeline)
            : spl::parse_pipeline_file(pipeline);
        const auto result = graph.prepare().execute(inputs, &runtimes, options);
        if (!result.succeeded()) {
            std::cerr << result.error->to_string() << '\n';
            return 1;
        }
        for (const auto& [name, value] : result.outputs) std::cout << name << ": " << value.to_string() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "SPL error: " << error.what() << '\n';
        return 1;
    }
}
