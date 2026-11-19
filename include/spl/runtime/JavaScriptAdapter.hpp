#pragma once

#include "RuntimeAdapter.hpp"

#include <string>

namespace spl::runtime {

class JavaScriptAdapter final : public RuntimeAdapter {
public:
    explicit JavaScriptAdapter(
        std::string node_executable = "node",
        std::string worker_path = "runtimes/javascript/worker.cjs");

    std::string id() const override;

    NodeResult invoke(
        const Implementation& implementation,
        const Values& inputs,
        const ExecutionContext& context) override;

private:
    std::string node_executable_;
    std::string worker_path_;
};

} // namespace spl::runtime
