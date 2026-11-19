#pragma once

#include <spl/Error.hpp>
#include <spl/Type.hpp>

#include <string>
#include <vector>

namespace spl::runtime {

struct PortSpec {
    std::string name;
    Type type;
    bool required = true;
};

struct ExecutionContext {
    std::string invocation_id;
    std::vector<PortSpec> inputs;
    std::vector<PortSpec> outputs;
};

struct Implementation {
    std::string runtime;
    std::string target;
};

class RuntimeAdapter {
public:
    virtual ~RuntimeAdapter() = default;

    virtual std::string id() const = 0;

    virtual NodeResult invoke(
        const Implementation& implementation,
        const Values& inputs,
        const ExecutionContext& context) = 0;
};

} // namespace spl::runtime
