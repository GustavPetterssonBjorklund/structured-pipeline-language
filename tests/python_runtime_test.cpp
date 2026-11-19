#include "support/TestAssertions.hpp"

#include <spl/Parser.hpp>
#include <spl/runtime/PythonAdapter.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>

int main() {
    using namespace spl;
    using namespace spl::test;

    const auto graph = parse_pipeline_file(std::filesystem::path{"examples/pipeline.spl"});
    runtime::RuntimeRegistry runtimes;
    runtimes.register_adapter(std::make_unique<runtime::PythonAdapter>());
    const auto result = graph.execute(
        Values{{"x", Value{std::int64_t{3}}}, {"y", Value{std::int64_t{4}}}}, &runtimes);
    require(result.outputs.at("result").get<std::int64_t>() == 14, "Python runtime result was incorrect");
}
