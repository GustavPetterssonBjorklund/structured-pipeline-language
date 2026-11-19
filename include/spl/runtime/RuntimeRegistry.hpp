#pragma once

#include "RuntimeAdapter.hpp"

#include <map>
#include <memory>
#include <stdexcept>

namespace spl::runtime {

class RegistryError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class RuntimeRegistry {
public:
    void register_adapter(std::unique_ptr<RuntimeAdapter> adapter) {
        if (!adapter) throw RegistryError("cannot register a null runtime adapter");
        const auto name = adapter->id();
        if (name.empty()) throw RegistryError("runtime adapter must have an id");
        if (adapters_.contains(name)) throw RegistryError("duplicate runtime adapter: " + name);
        adapters_.emplace(name, std::move(adapter));
    }

    NodeResult invoke(
        const Implementation& implementation,
        const Values& inputs,
        const ExecutionContext& context) const {
        const auto adapter = adapters_.find(implementation.runtime);
        if (adapter == adapters_.end()) {
            throw RegistryError("no runtime adapter registered for: " + implementation.runtime);
        }
        return adapter->second->invoke(implementation, inputs, context);
    }

private:
    std::map<std::string, std::unique_ptr<RuntimeAdapter>> adapters_;
};

} // namespace spl::runtime
