#include <spl/Parser.hpp>
#include <spl/runtime/PythonAdapter.hpp>

#include <iostream>
#include <memory>

int main(int argc, char** argv) {
    try {
        const std::filesystem::path pipeline = argc > 1 ? argv[1] : "examples/pipeline.spl";
        spl::Values inputs{{"x", spl::Value{std::int64_t{3}}}, {"y", spl::Value{std::int64_t{4}}}};
        spl::runtime::RuntimeRegistry runtimes;
        runtimes.register_adapter(std::make_unique<spl::runtime::PythonAdapter>());
        const auto graph = spl::parse_pipeline_file(pipeline);
        const auto result = graph.execute(inputs, &runtimes);
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
