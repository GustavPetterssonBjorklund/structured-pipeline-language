#include <spl/PreparedGraph.hpp>
#include <spl/logging/Logger.hpp>

#include <chrono>

namespace spl {

PreparedGraph::PreparedGraph(Graph graph)
    : graph_(std::move(graph)) {
    graph_.validate();
    order_ = graph_.topological_order();
    std::size_t input_slot = 0;
    for (const auto& [name, type] : graph_.inputs_) {
        (void)type;
        input_slots_.emplace(name, input_slot++);
    }
    std::map<std::string, std::size_t> node_slots;
    for (std::size_t slot = 0; slot < order_.size(); ++slot) node_slots.emplace(order_[slot], slot);
    for (const auto& id : order_) {
        const auto& instance = graph_.instances_.at(id);
        const auto* definition = &graph_.definitions_.at(instance.definition_id);
        PreparedNode node{id, definition, {}};
        for (const auto& [name, binding] : instance.inputs) {
            node.inputs.emplace_back(name, prepare_binding(graph_, binding, input_slots_, node_slots));
        }
        nodes_.push_back(std::move(node));
    }
    for (const auto& [name, output] : graph_.outputs_) {
        outputs_.push_back(PreparedOutput{name, output.type, prepare_binding(graph_, output.binding, input_slots_, node_slots)});
    }
}

PreparedGraph::PreparedBinding PreparedGraph::prepare_binding(
    const Graph& graph,
    const Binding& binding,
    const std::map<std::string, std::size_t>& input_slots,
    const std::map<std::string, std::size_t>& node_slots) {
    PreparedBinding prepared;
    prepared.kind = binding.kind;
    if (binding.kind == Binding::Kind::Literal) prepared.literal = binding.literal;
    else if (binding.kind == Binding::Kind::PipelineInput) prepared.input_slot = input_slots.at(binding.name);
    else {
        prepared.node_slot = node_slots.at(binding.node_id);
        prepared.port_name = binding.port_name;
    }
    (void)graph;
    return prepared;
}

const std::map<std::string, Type>& PreparedGraph::input_types() const noexcept {
    return graph_.inputs_;
}

std::map<std::string, Type> PreparedGraph::output_types() const {
    return graph_.output_types();
}

ExecutionResult PreparedGraph::execute(
    const Values& pipeline_inputs,
    const runtime::RuntimeRegistry* runtime_registry,
    bool collect_trace) const {
    for (const auto& [name, type] : graph_.inputs_) {
        const auto value = pipeline_inputs.find(name);
        if (value == pipeline_inputs.end()) throw GraphError("missing pipeline input: " + name);
        Graph::require_type(value->second, type, "pipeline input '" + name + "'");
    }

    std::vector<Value> input_values;
    input_values.reserve(graph_.inputs_.size());
    for (const auto& [name, slot] : input_slots_) {
        (void)slot;
        input_values.push_back(pipeline_inputs.at(name));
    }

    ExecutionResult result;
    const auto pipeline_started = std::chrono::steady_clock::now();
    const std::string run_id = "prepared-run-" + std::to_string(++prepared_run_counter);
    const auto logger = logging::Logger::module("engine.graph");
    logger.info("pipeline started: {}", run_id);
    std::vector<Values> node_outputs(nodes_.size());
    for (std::size_t node_slot = 0; node_slot < nodes_.size(); ++node_slot) {
        const auto& node = nodes_[node_slot];
        const auto& node_id = node.id;
        const auto& definition = *node.definition;
        Values node_inputs;
        for (const auto& [name, binding] : node.inputs) {
            if (binding.kind == Binding::Kind::Literal) node_inputs.emplace(name, binding.literal);
            else if (binding.kind == Binding::Kind::PipelineInput) node_inputs.emplace(name, input_values[binding.input_slot]);
            else node_inputs.emplace(name, node_outputs[binding.node_slot].at(binding.port_name));
        }

        TraceEvent event;
        const auto node_started = std::chrono::steady_clock::now();
        logger.debug("node started: {}", node_id);
        if (collect_trace) {
            event.node_id = node_id;
            event.node_definition_id = definition.id;
            event.run_id = run_id;
            event.invocation_id = run_id + ":" + node_id;
            event.status = "running";
            event.inputs = node_inputs;
        }
        try {
            NodeResult node_result;
            if (definition.runtime_implementation) {
                if (!runtime_registry) throw GraphError("node '" + node_id + "' requires a runtime registry");
                runtime::ExecutionContext context{event.invocation_id, run_id, node_id, definition.id, {}, {}};
                for (const auto& port : definition.ports) {
                    auto& target = port.direction == PortDirection::Input ? context.inputs : context.outputs;
                    target.push_back(runtime::PortSpec{port.name, port.type, port.required});
                }
                node_result = runtime_registry->invoke(*definition.runtime_implementation, node_inputs, context);
            } else {
                node_result = definition.implementation(node_inputs);
            }
            if (std::holds_alternative<ErrorValue>(node_result)) {
                const auto& error = std::get<ErrorValue>(node_result);
                if (std::find(definition.error_types.begin(), definition.error_types.end(), error.definition_id()) == definition.error_types.end()) {
                    throw GraphError("node '" + node_id + "' returned undeclared error: " + error.definition_id());
                }
                if (collect_trace) {
                    event.status = "failed";
                    event.event_kind = trace::TraceEventKind::NodeFailed;
                    event.duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - node_started).count();
                    event.error = error.to_string();
                    event.typed_error = error;
                    result.trace.push_back(std::move(event));
                }
                result.error = error;
                return result;
            }
            auto outputs = std::get<Values>(std::move(node_result));
            for (const auto& [name, value] : outputs) {
                (void)value;
                if (!definition.port(name, PortDirection::Output)) throw GraphError("unexpected output '" + name + "' from node '" + node_id + "'");
            }
            for (const auto& port : definition.ports) {
                if (port.direction != PortDirection::Output) continue;
                const auto output = outputs.find(port.name);
                if (output == outputs.end()) throw GraphError("missing output '" + port.name + "' from node '" + node_id + "'");
                Graph::require_type(output->second, port.type, "output '" + node_id + "." + port.name + "'");
            }
            if (collect_trace) {
                event.status = "succeeded";
                event.event_kind = trace::TraceEventKind::NodeCompleted;
                event.duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - node_started).count();
                event.outputs = outputs;
            }
            node_outputs[node_slot] = std::move(outputs);
        } catch (const std::exception& error) {
            if (collect_trace) {
                event.status = "failed";
                event.error = error.what();
                event.event_kind = trace::TraceEventKind::NodeFailed;
                event.duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - node_started).count();
                result.trace.push_back(std::move(event));
            }
            throw;
        }
        if (collect_trace) result.trace.push_back(std::move(event));
    }
    for (const auto& output : outputs_) {
        Value value = output.binding.kind == Binding::Kind::Literal
            ? output.binding.literal
            : output.binding.kind == Binding::Kind::PipelineInput
                ? input_values[output.binding.input_slot]
                : node_outputs[output.binding.node_slot].at(output.binding.port_name);
        if (output.type) Graph::require_type(value, *output.type, "pipeline output '" + output.name + "'");
        result.outputs.emplace(output.name, value);
    }
    result.statistics.pipeline_duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - pipeline_started).count();
    logger.info("pipeline completed: {}", run_id);
    return result;
}

ExecutionResult PreparedGraph::execute(
    const Values& pipeline_inputs,
    const runtime::RuntimeRegistry* runtime_registry,
    const trace::ExecutionOptions& options) const {
    auto result = execute(pipeline_inputs, runtime_registry, options.observability.mode != trace::ObservabilityMode::Off);
    for (const auto& event : result.trace) {
        trace::TraceEvent structured;
        structured.kind = event.status == "succeeded" ? trace::TraceEventKind::NodeCompleted : trace::TraceEventKind::NodeFailed;
        structured.node_instance_id = event.node_id;
        structured.node_definition_id = event.node_definition_id;
        structured.invocation_id = event.invocation_id;
        structured.duration_ns = event.duration_ns;
        structured.inputs = event.inputs;
        structured.outputs = event.outputs;
        structured.error = event.error;
        structured.typed_error = event.typed_error;
        observability::publish_trace(structured);
    }
    if (options.collector) {
        for (const auto& event : result.trace) {
            trace::TraceEvent structured;
            structured.kind = event.status == "succeeded" ? trace::TraceEventKind::NodeCompleted : trace::TraceEventKind::NodeFailed;
            structured.node_instance_id = event.node_id;
            structured.node_definition_id = event.node_definition_id;
            structured.invocation_id = event.invocation_id;
            structured.duration_ns = event.duration_ns;
            structured.inputs = event.inputs;
            structured.outputs = event.outputs;
            structured.error = event.error;
            structured.typed_error = event.typed_error;
            options.collector->emit(structured);
        }
    }
    for (const auto& sink : options.trace_sinks) {
        if (!sink) continue;
        for (const auto& event : result.trace) {
            trace::TraceEvent structured;
            structured.kind = event.status == "succeeded" ? trace::TraceEventKind::NodeCompleted : trace::TraceEventKind::NodeFailed;
            structured.node_instance_id = event.node_id;
            structured.node_definition_id = event.node_definition_id;
            structured.invocation_id = event.invocation_id;
            structured.duration_ns = event.duration_ns;
            structured.inputs = event.inputs;
            structured.outputs = event.outputs;
            structured.error = event.error;
            structured.typed_error = event.typed_error;
            sink->emit(structured);
        }
    }
    return result;
}

PreparedGraph Graph::prepare() const { return PreparedGraph(*this); }

} // namespace spl
