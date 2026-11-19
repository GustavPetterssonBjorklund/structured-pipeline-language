#include "support/TestAssertions.hpp"

#include <spl/Parser.hpp>
#include <spl/runtime/JavaScriptAdapter.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>

int main(int argc, char**) {
    using namespace spl;
    using namespace spl::test;
    const auto source = std::filesystem::temp_directory_path() / "spl-javascript-test.spl";
    {
        std::ofstream file(source);
        const auto fixture = argc > 1
            ? std::filesystem::absolute("tests/fixtures/typescript_nodes.ts")
            : std::filesystem::absolute("tests/fixtures/javascript_nodes.js");
        file << "node Add { input a: Int64 input b: Int64 output sum: Int64 impl javascript \"";
        file << fixture.string() << "::add\" }\n";
        file << "pipeline Main { input x: Int64 input y: Int64 output result: Int64 use add: Add(a = x, b = y) result = add.sum }\n";
    }
    const auto graph = parse_pipeline_file(source);
    runtime::RuntimeRegistry runtimes;
    runtimes.register_adapter(std::make_unique<runtime::JavaScriptAdapter>());
    const auto result = graph.execute(Values{{"x", Value{std::int64_t{3}}}, {"y", Value{std::int64_t{4}}}}, &runtimes);
    require(result.outputs.at("result").get<std::int64_t>() == 7, "JavaScript runtime result was incorrect");
    std::filesystem::remove(source);
}
