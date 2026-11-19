#pragma once

#include "RuntimeAdapter.hpp"

namespace spl::runtime {

class PythonAdapter final : public RuntimeAdapter {
public:
    explicit PythonAdapter(std::string python_package_path = "adapters/python");
    ~PythonAdapter() override;

    std::string id() const override;

    NodeResult invoke(
        const Implementation& implementation,
        const Values& inputs,
        const ExecutionContext& context) override;

private:
    std::string python_package_path_;
};

} // namespace spl::runtime
