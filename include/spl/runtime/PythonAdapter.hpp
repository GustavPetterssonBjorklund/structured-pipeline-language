#pragma once

#include "RuntimeAdapter.hpp"

namespace spl::runtime {

class PythonAdapter final : public RuntimeAdapter {
public:
    explicit PythonAdapter(std::string worker_path = "runtimes/python/worker.py");

    std::string id() const override;

    NodeResult invoke(
        const Implementation& implementation,
        const Values& inputs,
        const ExecutionContext& context) override;

private:
    std::string worker_path_;
};

} // namespace spl::runtime
