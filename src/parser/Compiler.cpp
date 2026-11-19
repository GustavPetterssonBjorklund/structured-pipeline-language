#include "Compiler.hpp"

#include <map>
#include <memory>
#include <set>

namespace spl::parser {
namespace {

Type type_from_name(const std::string& name) {
    if (name == "Int64") return Type::Int64;
    if (name == "Float64") return Type::Float64;
    if (name == "Bool") return Type::Bool;
    if (name == "String") return Type::String;
    throw GraphError("unknown primitive type: " + name);
}

Binding compile_binding(const BindingSyntax& binding) {
    switch (binding.kind) {
        case BindingSyntax::Kind::Integer: return Binding::literal_value(Value{static_cast<std::int64_t>(std::stoll(binding.value))});
        case BindingSyntax::Kind::Float: return Binding::literal_value(Value{std::stod(binding.value)});
        case BindingSyntax::Kind::Bool: return Binding::literal_value(Value{binding.value == "true"});
        case BindingSyntax::Kind::String: return Binding::literal_value(Value{binding.value});
        case BindingSyntax::Kind::Reference:
            return binding.port.empty() ? Binding::pipeline_input(binding.value) : Binding::node_output(binding.value, binding.port);
    }
    throw GraphError("unsupported binding");
}

} // namespace

namespace {

std::shared_ptr<Graph> compile_pipeline(
    const ProgramSyntax& program,
    const std::filesystem::path& source_path,
    const PipelineSyntax& pipeline,
    std::map<std::string, std::shared_ptr<Graph>>& cache,
    std::set<std::string>& active) {
    if (const auto cached = cache.find(pipeline.id); cached != cache.end()) return cached->second;
    if (!active.insert(pipeline.id).second) throw GraphError("pipeline composition contains a cycle: " + pipeline.id);

    auto graph = std::make_shared<Graph>();
    graph->set_pipeline_name(pipeline.id);
    for (const auto& node : program.nodes) {
        NodeDefinition definition;
        definition.id = node.id;
        for (const auto& port : node.ports) {
            definition.ports.push_back(Port{port.name, port.input ? PortDirection::Input : PortDirection::Output, type_from_name(port.type)});
        }
        definition.runtime_implementation = runtime::Implementation{
            node.runtime, std::filesystem::absolute(source_path.parent_path() / node.target).lexically_normal().string()};
        graph->add_definition(std::move(definition));
    }

    for (const auto& input : pipeline.inputs) graph->add_input(input.name, type_from_name(input.type));
    for (const auto& instance : pipeline.instances) {
        if (!graph->has_definition(instance.definition)) {
            const auto child = std::find_if(program.pipelines.begin(), program.pipelines.end(), [&](const auto& candidate) {
                return candidate.id == instance.definition;
            });
            if (child == program.pipelines.end()) throw GraphError("unknown node or pipeline definition: " + instance.definition);
            const auto child_graph = compile_pipeline(program, source_path, *child, cache, active);
            NodeDefinition definition;
            definition.id = child->id;
            for (const auto& [name, type] : child_graph->input_types()) definition.ports.push_back(Port{name, PortDirection::Input, type});
            for (const auto& [name, type] : child_graph->output_types()) definition.ports.push_back(Port{name, PortDirection::Output, type});
            definition.pipeline_implementation = child_graph;
            graph->add_definition(std::move(definition));
        }
        NodeInstance compiled{instance.id, instance.definition, {}};
        for (const auto& [name, binding] : instance.arguments) {
            if (!compiled.inputs.emplace(name, compile_binding(binding)).second) throw GraphError("duplicate node argument: " + name);
        }
        graph->add_instance(std::move(compiled));
    }
    for (const auto& output : pipeline.outputs) {
        graph->add_output(output.name, type_from_name(output.type), compile_binding(output.binding));
    }
    graph->validate();
    active.erase(pipeline.id);
    cache.emplace(pipeline.id, graph);
    return graph;
}

} // namespace

Graph compile(const ProgramSyntax& program, const std::filesystem::path& source_path,
              std::string_view pipeline_name) {
    const PipelineSyntax* selected = nullptr;
    for (const auto& candidate : program.pipelines) {
        if (pipeline_name.empty() || candidate.id == pipeline_name) {
            if (selected != nullptr) {
                if (pipeline_name.empty()) throw GraphError("multiple pipelines found; specify a pipeline name");
                throw GraphError("duplicate pipeline name: " + std::string(pipeline_name));
            }
            selected = &candidate;
        }
    }
    if (selected == nullptr) {
        throw GraphError("unknown pipeline: " + std::string(pipeline_name));
    }
    std::map<std::string, std::shared_ptr<Graph>> cache;
    std::set<std::string> active;
    return *compile_pipeline(program, source_path, *selected, cache, active);
}

} // namespace spl::parser
