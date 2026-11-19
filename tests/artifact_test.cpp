#include "support/TestAssertions.hpp"

#include <spl/Artifact.hpp>
#include <spl/Parser.hpp>
#include <spl/PreparedGraph.hpp>

#include <filesystem>
#include <cstdint>

int main() {
    using namespace spl;
    using namespace spl::test;

    const auto artifact = std::filesystem::temp_directory_path() / "spl-artifact-test.splb";
    const auto graph = parse_pipeline_file(std::filesystem::path{"tests/fixtures/literals.spl"});
    write_pipeline_artifact(graph, artifact);
    const auto restored = read_pipeline_artifact(artifact);
    const auto prepared = restored.prepare();
    const auto result = prepared.execute({});
    const auto repeated = prepared.execute({});
    require(result.outputs.at("flag").get<bool>(), "artifact Bool output was incorrect");
    require(result.outputs.at("score").get<double>() == 3.5, "artifact Float64 output was incorrect");
    require(repeated.outputs.at("score").get<double>() == 3.5, "repeated prepared execution was incorrect");
    std::filesystem::remove(artifact);

    const auto runtime_artifact = std::filesystem::temp_directory_path() / "spl-runtime-artifact-test.splb";
    write_pipeline_artifact(parse_pipeline_file(std::filesystem::path{"examples/pipeline.spl"}), runtime_artifact);
    read_pipeline_artifact(runtime_artifact).prepare();
    std::filesystem::remove(runtime_artifact);
}
