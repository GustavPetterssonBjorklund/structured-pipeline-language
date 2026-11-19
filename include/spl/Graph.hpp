#pragma once

#include "Value.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace spl {

enum class PortDirection { Input, Output };

struct Port {
    std::string name;
    PortDirection direction;
    Type type;
    bool required = true;
};

using Values = std::map<std::string, Value>;
using NodeFunction = std::function<Values(const Values&)>;

struct NodeDefinition {
    std::string id;
    std::vector<Port> ports;
    NodeFunction implementation;

    const Port* port(const std::string& name, PortDirection direction) const {
        for (const auto& candidate : ports) {
            if (candidate.name == name && candidate.direction == direction) {
                return &candidate;
            }
        }
        return nullptr;
    }
};

struct Binding {
    enum class Kind { Literal, PipelineInput, NodeOutput };

    Kind kind;
    Value literal{std::int64_t{0}};
    std::string name;
    std::string node_id;
    std::string port_name;

    static Binding literal_value(Value value) {
        return Binding{Kind::Literal, std::move(value), {}, {}, {}};
    }

    static Binding pipeline_input(std::string name) {
        return Binding{Kind::PipelineInput, Value{std::int64_t{0}}, std::move(name), {}, {}};
    }

    static Binding node_output(std::string node_id, std::string port_name) {
        return Binding{Kind::NodeOutput, Value{std::int64_t{0}}, {}, std::move(node_id), std::move(port_name)};
    }
};

struct NodeInstance {
    std::string id;
    std::string definition_id;
    std::map<std::string, Binding> inputs;
};

struct TraceEvent {
    std::string node_id;
    std::string status;
    Values inputs;
    Values outputs;
    std::string error;
};

struct ExecutionResult {
    Values outputs;
    std::vector<TraceEvent> trace;
};

class GraphError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Graph {
public:
    void add_definition(NodeDefinition definition) {
        if (definition.id.empty() || !definition.implementation) {
            throw GraphError("node definition must have an id and implementation");
        }
        if (definitions_.contains(definition.id)) {
            throw GraphError("duplicate node definition: " + definition.id);
        }
        validate_ports(definition);
        definitions_.emplace(definition.id, std::move(definition));
    }

    void add_input(std::string name, Type type) {
        if (inputs_.contains(name)) throw GraphError("duplicate pipeline input: " + name);
        inputs_.emplace(std::move(name), type);
    }

    void add_output(std::string name, Binding binding) {
        if (outputs_.contains(name)) throw GraphError("duplicate pipeline output: " + name);
        outputs_.emplace(std::move(name), std::move(binding));
    }

    void add_instance(NodeInstance instance) {
        if (instance.id.empty()) throw GraphError("node instance must have an id");
        if (instances_.contains(instance.id)) throw GraphError("duplicate node instance: " + instance.id);
        if (!definitions_.contains(instance.definition_id)) {
            throw GraphError("unknown node definition: " + instance.definition_id);
        }
        instances_.emplace(std::move(instance.id), std::move(instance));
    }

    void validate() const {
        for (const auto& [id, instance] : instances_) {
            const auto& definition = definitions_.at(instance.definition_id);
            for (const auto& port : definition.ports) {
                if (port.direction != PortDirection::Input) continue;
                const auto binding = instance.inputs.find(port.name);
                if (port.required && binding == instance.inputs.end()) {
                    throw GraphError("missing input '" + port.name + "' on node '" + id + "'");
                }
                if (binding != instance.inputs.end()) validate_binding(binding->second, port.type);
            }
            for (const auto& [name, binding] : instance.inputs) {
                if (!definition.port(name, PortDirection::Input)) {
                    throw GraphError("unknown input '" + name + "' on node '" + id + "'");
                }
                (void)binding;
            }
        }
        for (const auto& [name, binding] : outputs_) {
            validate_output_binding(name, binding);
        }
        (void)topological_order();
    }

    ExecutionResult execute(const Values& pipeline_inputs) const {
        validate();
        for (const auto& [name, type] : inputs_) {
            const auto value = pipeline_inputs.find(name);
            if (value == pipeline_inputs.end()) throw GraphError("missing pipeline input: " + name);
            require_type(value->second, type, "pipeline input '" + name + "'");
        }

        ExecutionResult result;
        std::map<std::string, Values> node_outputs;
        for (const auto& node_id : topological_order()) {
            const auto& instance = instances_.at(node_id);
            const auto& definition = definitions_.at(instance.definition_id);
            Values node_inputs;
            for (const auto& port : definition.ports) {
                if (port.direction != PortDirection::Input) continue;
                const auto binding = instance.inputs.find(port.name);
                if (binding == instance.inputs.end()) continue;
                node_inputs.emplace(port.name, resolve(binding->second, pipeline_inputs, node_outputs));
            }

            TraceEvent event;
            event.node_id = node_id;
            event.status = "running";
            event.inputs = node_inputs;
            try {
                auto outputs = definition.implementation(node_inputs);
                for (const auto& port : definition.ports) {
                    if (port.direction != PortDirection::Output) continue;
                    const auto output = outputs.find(port.name);
                    if (output == outputs.end()) throw GraphError("missing output '" + port.name + "' from node '" + node_id + "'");
                    require_type(output->second, port.type, "output '" + node_id + "." + port.name + "'");
                }
                event.status = "succeeded";
                event.outputs = outputs;
                node_outputs.emplace(node_id, std::move(outputs));
            } catch (const std::exception& error) {
                event.status = "failed";
                event.error = error.what();
                result.trace.push_back(std::move(event));
                throw;
            }
            result.trace.push_back(std::move(event));
        }

        for (const auto& [name, binding] : outputs_) {
            result.outputs.emplace(name, resolve(binding, pipeline_inputs, node_outputs));
        }
        return result;
    }

private:
    static void validate_ports(const NodeDefinition& definition) {
        std::set<std::string> names;
        for (const auto& port : definition.ports) {
            if (!names.insert(std::to_string(static_cast<int>(port.direction)) + ":" + port.name).second) {
                throw GraphError("duplicate port: " + port.name);
            }
        }
    }

    void validate_binding(const Binding& binding, Type expected) const {
        if (binding.kind == Binding::Kind::Literal) require_type(binding.literal, expected, "literal binding");
        if (binding.kind == Binding::Kind::PipelineInput) {
            const auto input = inputs_.find(binding.name);
            if (input == inputs_.end()) throw GraphError("unknown pipeline input: " + binding.name);
            if (input->second != expected) throw GraphError("type mismatch for pipeline input: " + binding.name);
        }
        if (binding.kind == Binding::Kind::NodeOutput) {
            const auto node = instances_.find(binding.node_id);
            if (node == instances_.end()) throw GraphError("unknown source node: " + binding.node_id);
            const auto& definition = definitions_.at(node->second.definition_id);
            const auto* port = definition.port(binding.port_name, PortDirection::Output);
            if (!port) throw GraphError("unknown output: " + binding.node_id + "." + binding.port_name);
            if (port->type != expected) throw GraphError("type mismatch for node output: " + binding.node_id + "." + binding.port_name);
        }
    }

    void validate_output_binding(const std::string& name, const Binding& binding) const {
        (void)name;
        if (binding.kind == Binding::Kind::PipelineInput) {
            if (!inputs_.contains(binding.name)) throw GraphError("unknown pipeline input: " + binding.name);
        } else if (binding.kind == Binding::Kind::NodeOutput) {
            const auto node = instances_.find(binding.node_id);
            if (node == instances_.end()) throw GraphError("unknown output source node: " + binding.node_id);
            if (!definitions_.at(node->second.definition_id).port(binding.port_name, PortDirection::Output)) {
                throw GraphError("unknown output: " + binding.node_id + "." + binding.port_name);
            }
        }
    }

    std::vector<std::string> topological_order() const {
        std::map<std::string, std::set<std::string>> dependencies;
        for (const auto& [id, instance] : instances_) {
            for (const auto& [name, binding] : instance.inputs) {
                (void)name;
                if (binding.kind == Binding::Kind::NodeOutput) dependencies[id].insert(binding.node_id);
            }
        }
        std::vector<std::string> order;
        std::set<std::string> remaining;
        for (const auto& [id, instance] : instances_) {
            (void)instance;
            remaining.insert(id);
        }
        while (!remaining.empty()) {
            auto ready = std::find_if(remaining.begin(), remaining.end(), [&](const auto& id) {
                for (const auto& dependency : dependencies[id]) if (remaining.contains(dependency)) return false;
                return true;
            });
            if (ready == remaining.end()) throw GraphError("graph contains a cycle");
            order.push_back(*ready);
            remaining.erase(ready);
        }
        return order;
    }

    static void require_type(const Value& value, Type expected, const std::string& context) {
        if (value.type() != expected) {
            throw GraphError("type mismatch in " + context + ": expected " + std::string(type_name(expected)) + ", got " + std::string(type_name(value.type())));
        }
    }

    static Value resolve(const Binding& binding, const Values& pipeline_inputs, const std::map<std::string, Values>& node_outputs) {
        if (binding.kind == Binding::Kind::Literal) return binding.literal;
        if (binding.kind == Binding::Kind::PipelineInput) return pipeline_inputs.at(binding.name);
        return node_outputs.at(binding.node_id).at(binding.port_name);
    }

    std::map<std::string, NodeDefinition> definitions_;
    std::map<std::string, Type> inputs_;
    std::map<std::string, NodeInstance> instances_;
    std::map<std::string, Binding> outputs_;
};

} // namespace spl
