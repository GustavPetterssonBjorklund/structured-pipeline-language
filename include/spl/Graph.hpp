#pragma once

#include "Error.hpp"
#include "runtime/RuntimeRegistry.hpp"
#include "trace/TraceConfig.hpp"
#include "trace/TraceCollector.hpp"
#include "trace/TraceEvent.hpp"
#include "trace/TraceSink.hpp"
#include "Observability.hpp"
#include "logging/Logger.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <chrono>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace spl {

class PreparedGraph;

enum class PortDirection { Input, Output };

struct Port {
    std::string name;
    PortDirection direction;
    Type type;
    bool required = true;
};

using NodeFunction = std::function<NodeResult(const Values&)>;

struct NodeDefinition {
    std::string id;
    std::vector<Port> ports;
    NodeFunction implementation;
    std::vector<std::string> error_types;
    std::optional<runtime::Implementation> runtime_implementation;

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
    std::optional<ErrorValue> typed_error;
    trace::TraceEventKind event_kind = trace::TraceEventKind::NodeCompleted;
    std::string run_id;
    std::string invocation_id;
    std::string runtime_id;
    std::string timestamp;
    std::int64_t duration_ns = 0;
    std::string node_definition_id;
};

struct NodeStatistics {
    std::string node_id;
    std::string runtime;
    std::int64_t execution_duration_ns = 0;
    std::int64_t runtime_duration_ns = 0;
    std::int64_t validation_duration_ns = 0;
    std::size_t input_count = 0;
    std::size_t output_count = 0;
    std::string status;
    std::string error;
};

struct ExecutionStatistics {
    std::int64_t pipeline_duration_ns = 0;
    std::size_t successful_nodes = 0;
    std::size_t failed_nodes = 0;
    std::vector<NodeStatistics> nodes;
};

struct PipelineOutput {
    std::optional<Type> type;
    Binding binding;
};

struct ExecutionResult {
    Values outputs;
    std::vector<TraceEvent> trace;
    std::optional<ErrorValue> error;
    ExecutionStatistics statistics;

    bool succeeded() const noexcept { return !error.has_value(); }
};

class GraphError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Graph;
void write_pipeline_artifact(const Graph& graph, const std::filesystem::path& path);
Graph read_pipeline_artifact(const std::filesystem::path& path);

class Graph {
public:
    PreparedGraph prepare() const;
    const std::string& pipeline_name() const noexcept { return pipeline_name_; }
    void set_pipeline_name(std::string name) { pipeline_name_ = std::move(name); }
    const std::map<std::string, Type>& input_types() const noexcept { return inputs_; }

    std::map<std::string, Type> output_types() const {
        std::map<std::string, Type> result;
        for (const auto& [name, output] : outputs_) {
            if (!output.type) throw GraphError("pipeline output has no type: " + name);
            result.emplace(name, *output.type);
        }
        return result;
    }

    void add_definition(NodeDefinition definition) {
        if (definition.id.empty() || (!definition.implementation && !definition.runtime_implementation)) {
            throw GraphError("node definition must have an id and implementation");
        }
        if (definitions_.contains(definition.id)) {
            throw GraphError("duplicate node definition: " + definition.id);
        }
        validate_ports(definition);
        definitions_.emplace(definition.id, std::move(definition));
    }

    void add_error_definition(ErrorDefinition definition) {
        if (definition.id.empty()) throw GraphError("error definition must have an id");
        if (errors_.contains(definition.id)) throw GraphError("duplicate error definition: " + definition.id);
        std::set<std::string> fields;
        for (const auto& field : definition.fields) {
            if (field.name.empty() || !fields.insert(field.name).second) {
                throw GraphError("duplicate or empty error field in: " + definition.id);
            }
        }
        errors_.emplace(std::move(definition.id), std::move(definition));
    }

    void add_input(std::string name, Type type) {
        if (inputs_.contains(name)) throw GraphError("duplicate pipeline input: " + name);
        inputs_.emplace(std::move(name), type);
    }

    void add_output(std::string name, Binding binding) {
        if (outputs_.contains(name)) throw GraphError("duplicate pipeline output: " + name);
        outputs_.emplace(std::move(name), PipelineOutput{std::nullopt, std::move(binding)});
    }

    void add_output(std::string name, Type type, Binding binding) {
        if (outputs_.contains(name)) throw GraphError("duplicate pipeline output: " + name);
        outputs_.emplace(std::move(name), PipelineOutput{type, std::move(binding)});
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
            for (const auto& error_type : definition.error_types) {
                if (!errors_.contains(error_type)) throw GraphError("unknown error type: " + error_type);
            }
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
        for (const auto& [name, output] : outputs_) {
            validate_output_binding(name, output.binding);
            if (output.type) validate_output_type(name, output.binding, *output.type);
        }
        (void)topological_order();
    }

    ExecutionResult execute(
        const Values& pipeline_inputs,
        const runtime::RuntimeRegistry* runtime_registry = nullptr) const {
        trace::ExecutionOptions compatibility;
        compatibility.observability.capture_inputs = true;
        compatibility.observability.capture_outputs = true;
        return execute(pipeline_inputs, runtime_registry, compatibility);
    }

    ExecutionResult execute(
        const Values& pipeline_inputs,
        const runtime::RuntimeRegistry* runtime_registry,
        const trace::ExecutionOptions& options) const {
        const auto started = std::chrono::steady_clock::now();
        const auto logger = logging::Logger::module("engine.graph");
        validate();
        for (const auto& [name, type] : inputs_) {
            const auto value = pipeline_inputs.find(name);
            if (value == pipeline_inputs.end()) throw GraphError("missing pipeline input: " + name);
            require_type(value->second, type, "pipeline input '" + name + "'");
        }

        ExecutionResult result;
        const std::string run_id = "run-" + std::to_string(++run_counter_);
        auto emit = [&](trace::TraceEventKind kind, const TraceEvent& source) {
            trace::TraceEvent structured;
            structured.kind = kind; structured.run_id = run_id; structured.pipeline_name = pipeline_name_;
            structured.node_instance_id = source.node_id; structured.node_definition_id = source.node_definition_id;
            structured.invocation_id = source.invocation_id; structured.runtime_id = source.runtime_id;
            structured.timestamp = source.timestamp; structured.duration_ns = source.duration_ns;
            auto safe_values = [&](const Values& values, bool output) {
                std::map<std::string, std::string> captured;
                for (const auto& [name, value] : values) {
                    std::string text = options.observability.redactor
                        ? options.observability.redactor(value, trace::ValueContext{source.node_id, name, output})
                        : value.to_string();
                    if (text.size() > options.observability.max_value_length) {
                        text.resize(options.observability.max_value_length);
                        text += "...";
                    }
                    captured.emplace(name, std::move(text));
                }
                return captured;
            };
            if (options.observability.mode == trace::ObservabilityMode::Debug || options.observability.capture_inputs) structured.input_values = safe_values(source.inputs, false);
            if (options.observability.mode == trace::ObservabilityMode::Debug || options.observability.capture_outputs) structured.output_values = safe_values(source.outputs, true);
            structured.inputs = source.inputs; structured.outputs = source.outputs;
            structured.error = source.error; structured.typed_error = source.typed_error;
            for (const auto& sink : options.trace_sinks) if (sink) sink->emit(structured);
            if (options.collector) options.collector->emit(structured);
            observability::publish_trace(structured);
        };
        std::map<std::string, Values> node_outputs;
        if (options.observability.mode != trace::ObservabilityMode::Off) emit(trace::TraceEventKind::PipelineStarted, TraceEvent{});
        logger.info("pipeline started: {} ({})", pipeline_name_, run_id);
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
            event.node_definition_id = definition.id;
            event.run_id = run_id;
            event.invocation_id = run_id + ":" + node_id;
            event.status = "running";
            if (options.observability.mode == trace::ObservabilityMode::Debug || options.observability.capture_inputs) event.inputs = node_inputs;
            const auto node_started = std::chrono::steady_clock::now();
            logger.debug("node started: {}", node_id);
            NodeStatistics node_statistics;
            node_statistics.node_id = node_id;
            node_statistics.runtime = definition.runtime_implementation ? definition.runtime_implementation->runtime : "native";
            node_statistics.input_count = node_inputs.size();
            if (options.observability.mode != trace::ObservabilityMode::Off) emit(trace::TraceEventKind::NodeStarted, event);
            try {
                NodeResult node_result;
                if (definition.runtime_implementation) {
                    if (!runtime_registry) {
                        throw GraphError("node '" + node_id + "' requires a runtime registry");
                    }
                    runtime::ExecutionContext context{event.invocation_id, run_id, node_id, definition.id, {}, {}};
                    for (const auto& port : definition.ports) {
                        if (port.direction == PortDirection::Input) {
                            context.inputs.push_back(runtime::PortSpec{port.name, port.type, port.required});
                        } else {
                            context.outputs.push_back(runtime::PortSpec{port.name, port.type, port.required});
                        }
                    }
                    const auto runtime_started = std::chrono::steady_clock::now();
                    if (options.observability.mode != trace::ObservabilityMode::Off) emit(trace::TraceEventKind::RuntimeInvoked, event);
                    node_result = runtime_registry->invoke(*definition.runtime_implementation, node_inputs, context);
                    event.runtime_id = definition.runtime_implementation->runtime;
                    event.duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - runtime_started).count();
                    if (options.observability.mode != trace::ObservabilityMode::Off) emit(trace::TraceEventKind::RuntimeCompleted, event);
                } else {
                    node_result = definition.implementation(node_inputs);
                }
                if (std::holds_alternative<ErrorValue>(node_result)) {
                    const auto& error = std::get<ErrorValue>(node_result);
                    if (std::find(definition.error_types.begin(), definition.error_types.end(), error.definition_id()) == definition.error_types.end()) {
                        throw GraphError("node '" + node_id + "' returned undeclared error: " + error.definition_id());
                    }
                    event.status = "failed";
                    event.error = error.to_string();
                    event.typed_error = error;
                    event.event_kind = trace::TraceEventKind::NodeFailed;
                    event.duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - node_started).count();
                    result.trace.push_back(std::move(event));
                    if (options.observability.mode != trace::ObservabilityMode::Off) emit(trace::TraceEventKind::NodeFailed, result.trace.back());
                    result.error = error;
                    result.statistics.failed_nodes++;
                    node_statistics.status = "failed";
                    node_statistics.error = event.error;
                    node_statistics.execution_duration_ns = event.duration_ns;
                    result.statistics.nodes.push_back(std::move(node_statistics));
                    result.statistics.pipeline_duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count();
                    if (options.observability.mode != trace::ObservabilityMode::Off) emit(trace::TraceEventKind::PipelineFailed, result.trace.back());
                    return result;
                }
                auto outputs = std::get<Values>(std::move(node_result));
                for (const auto& [name, value] : outputs) {
                    (void)value;
                    if (!definition.port(name, PortDirection::Output)) {
                        throw GraphError("unexpected output '" + name + "' from node '" + node_id + "'");
                    }
                }
                for (const auto& port : definition.ports) {
                    if (port.direction != PortDirection::Output) continue;
                    const auto output = outputs.find(port.name);
                    if (output == outputs.end()) throw GraphError("missing output '" + port.name + "' from node '" + node_id + "'");
                    require_type(output->second, port.type, "output '" + node_id + "." + port.name + "'");
                }
                event.status = "succeeded";
                if (options.observability.mode == trace::ObservabilityMode::Debug || options.observability.capture_outputs) event.outputs = outputs;
                event.event_kind = trace::TraceEventKind::NodeCompleted;
                event.duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - node_started).count();
                node_statistics.status = "succeeded";
                node_statistics.output_count = outputs.size();
                node_statistics.execution_duration_ns = event.duration_ns;
                node_outputs.emplace(node_id, std::move(outputs));
                result.statistics.successful_nodes++;
            } catch (const std::exception& error) {
                event.status = "failed";
                event.error = error.what();
                event.event_kind = trace::TraceEventKind::NodeFailed;
                event.duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - node_started).count();
                result.trace.push_back(std::move(event));
                if (options.observability.mode != trace::ObservabilityMode::Off) emit(trace::TraceEventKind::NodeFailed, result.trace.back());
                result.statistics.failed_nodes++;
                node_statistics.status = "failed";
                node_statistics.error = event.error;
                node_statistics.execution_duration_ns = event.duration_ns;
                result.statistics.nodes.push_back(std::move(node_statistics));
                if (options.observability.mode != trace::ObservabilityMode::Off) emit(trace::TraceEventKind::PipelineFailed, result.trace.back());
                throw;
            }
            result.trace.push_back(std::move(event));
            result.statistics.nodes.push_back(std::move(node_statistics));
            if (options.observability.mode != trace::ObservabilityMode::Off) emit(trace::TraceEventKind::NodeCompleted, result.trace.back());
        }

        for (const auto& [name, output] : outputs_) {
            const auto value = resolve(output.binding, pipeline_inputs, node_outputs);
            if (output.type) require_type(value, *output.type, "pipeline output '" + name + "'");
            result.outputs.emplace(name, value);
        }
        result.statistics.pipeline_duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count();
        if (options.observability.mode != trace::ObservabilityMode::Off) emit(trace::TraceEventKind::PipelineCompleted, TraceEvent{});
        logger.info("pipeline completed: {} ({})", pipeline_name_, run_id);
        return result;
    }

private:
    friend class PreparedGraph;
    friend void write_pipeline_artifact(const Graph&, const std::filesystem::path&);
    friend Graph read_pipeline_artifact(const std::filesystem::path&);

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

    void validate_output_type(const std::string& name, const Binding& binding, Type expected) const {
        if (binding.kind == Binding::Kind::Literal) require_type(binding.literal, expected, "pipeline output '" + name + "'");
        if (binding.kind == Binding::Kind::PipelineInput) {
            const auto input = inputs_.find(binding.name);
            if (input != inputs_.end() && input->second != expected) {
                throw GraphError("type mismatch for pipeline output '" + name + "'");
            }
        }
        if (binding.kind == Binding::Kind::NodeOutput) {
            const auto& instance = instances_.at(binding.node_id);
            const auto* port = definitions_.at(instance.definition_id).port(binding.port_name, PortDirection::Output);
            if (port && port->type != expected) throw GraphError("type mismatch for pipeline output '" + name + "'");
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
    std::map<std::string, ErrorDefinition> errors_;
    std::string pipeline_name_;
    std::map<std::string, Type> inputs_;
    std::map<std::string, NodeInstance> instances_;
    std::map<std::string, PipelineOutput> outputs_;
    inline static std::atomic<std::uint64_t> run_counter_{0};
};

} // namespace spl
