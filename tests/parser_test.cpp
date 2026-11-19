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
}
