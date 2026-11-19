#include <spl/runtime/PythonAdapter.hpp>

#include <cstdio>
#include <sstream>
#include <stdexcept>

namespace spl::runtime {
namespace {

std::string shell_quote(const std::string& value) {
    std::string quoted = "'";
    for (const char character : value) {
        if (character == '\'') quoted += "'\\''";
        else quoted += character;
    }
    return quoted + "'";
}

} // namespace

PythonAdapter::PythonAdapter(std::string worker_path) : worker_path_(std::move(worker_path)) {}

std::string PythonAdapter::id() const { return "python"; }

NodeResult PythonAdapter::invoke(
    const Implementation& implementation,
    const Values& inputs,
    const ExecutionContext&) {
    std::string command = "python3 " + shell_quote(worker_path_) + " --target " + shell_quote(implementation.target);
    for (const auto& [name, value] : inputs) {
        command += " --input " + shell_quote(name + "=" + value.to_string());
    }
    command += " 2>&1";

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) throw std::runtime_error("failed to start Python runtime");
    char buffer[4096];
    std::string output;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) output += buffer;
    const int status = pclose(pipe);
    if (status != 0) throw std::runtime_error("Python runtime failed: " + output);

    Values values;
    std::stringstream lines(output);
    std::string line;
    while (std::getline(lines, line)) {
        const auto equals = line.find('=');
        if (equals == std::string::npos) continue;
        values.emplace(line.substr(0, equals), Value{static_cast<std::int64_t>(std::stoll(line.substr(equals + 1)))});
    }
    return values;
}

} // namespace spl::runtime
