#include "support/TestAssertions.hpp"

#include <spl/Artifact.hpp>
#include <spl/Parser.hpp>
#include <spl/PreparedGraph.hpp>
#include <spl/runtime/JavaScriptAdapter.hpp>
#include <spl/runtime/PythonAdapter.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>

int main() {
    using namespace spl;
    using namespace spl::test;

    const auto source = std::filesystem::path{"tests/fixtures/nested_cross_language.spl"};
    const auto graph = parse_pipeline_file(source, "Main");
    runtime::RuntimeRegistry runtimes;
    runtimes.register_adapter(std::make_unique<runtime::PythonAdapter>());
    runtimes.register_adapter(std::make_unique<runtime::JavaScriptAdapter>());

    const Values inputs{{"x", Value{std::int64_t{3}}}, {"y", Value{std::int64_t{4}}}};
    const auto direct = graph.execute(inputs, &runtimes);
    require(direct.outputs.at("result").get<std::int64_t>() == 15, "nested mixed-runtime result was incorrect");

    const auto artifact = std::filesystem::temp_directory_path() / "spl-nested-runtime-test.splb";
    write_pipeline_artifact(graph, artifact);
    const auto restored = read_pipeline_artifact(artifact);
    const auto round_trip = restored.prepare().execute(inputs, &runtimes);
    require(round_trip.outputs.at("result").get<std::int64_t>() == 15, "nested artifact result was incorrect");
    std::filesystem::remove(artifact);
}
