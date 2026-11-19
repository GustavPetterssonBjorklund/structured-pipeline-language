#pragma once

#include "RuntimeAdapter.hpp"

#include <map>

struct _object;

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
    std::map<std::string, _object*> functions_;
    std::map<std::string, _object*> value_types_;
    bool package_path_added_ = false;
};

} // namespace spl::runtime
