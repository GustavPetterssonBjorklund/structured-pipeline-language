#include "support/TestAssertions.hpp"

#include <spl/Parser.hpp>

#include <filesystem>

int main() {
    using namespace spl;
    using namespace spl::test;

    const auto graph = parse_pipeline_file(std::filesystem::path{"examples/pipeline.spl"});
    graph.validate();

    const auto literals = parse_pipeline_file(std::filesystem::path{"tests/fixtures/literals.spl"});
    require(literals.execute({}).outputs.at("flag").get<bool>(), "bool literal was not parsed");
    require(literals.execute({}).outputs.at("score").get<double>() == 3.5, "float literal was not parsed");
    require(literals.execute({}).outputs.at("label").get<std::string>() == "hello", "string literal was not parsed");

    const auto from_source = parse_pipeline(
        "pipeline Inline { output answer: Int64 answer = 42 }",
        std::filesystem::path{"tests/fixtures/inline.spl"});
    require(from_source.execute({}).outputs.at("answer").get<std::int64_t>() == 42,
            "source-string parser entry point failed");

    const auto second = parse_pipeline_file(
        std::filesystem::path{"tests/fixtures/multiple_pipelines.spl"}, "Second");
    require(second.execute({}).outputs.at("answer").get<std::int64_t>() == 2,
            "named pipeline selection failed");
    require_graph_error([&] {
        (void)parse_pipeline_file(std::filesystem::path{"tests/fixtures/multiple_pipelines.spl"});
    }, "ambiguous pipeline selection was accepted");
}
