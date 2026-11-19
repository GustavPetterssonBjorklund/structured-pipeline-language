#pragma once

#include <spl/Error.hpp>

#include <string>

namespace spl::runtime {

struct ExecutionContext {
    std::string invocation_id;
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
