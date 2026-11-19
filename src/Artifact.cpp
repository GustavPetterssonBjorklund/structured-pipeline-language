#include <spl/Artifact.hpp>

#include <fstream>
#include <cstring>
#include <functional>
#include <stdexcept>

namespace spl {
namespace {

constexpr std::uint32_t artifact_version = 3;
constexpr std::uint64_t max_count = 1'000'000;
constexpr std::uint64_t max_string = 64 * 1024 * 1024;

class Writer {
public:
    explicit Writer(std::ostream& stream) : stream_(stream) {}

    void u8(std::uint8_t value) { stream_.put(static_cast<char>(value)); check(); }
    void u32(std::uint32_t value) { for (int i = 0; i < 4; ++i) u8(static_cast<std::uint8_t>(value >> (i * 8))); }
    void u64(std::uint64_t value) { for (int i = 0; i < 8; ++i) u8(static_cast<std::uint8_t>(value >> (i * 8))); }
    void string(const std::string& value) {
        u64(value.size());
        stream_.write(value.data(), static_cast<std::streamsize>(value.size()));
        check();
    }

private:
    void check() {
        if (!stream_) throw GraphError("cannot write SPL pipeline artifact");
    }
    std::ostream& stream_;
};

class Reader {
public:
    explicit Reader(std::istream& stream) : stream_(stream) {}

    std::uint8_t u8() {
        const auto value = stream_.get();
        if (value == std::char_traits<char>::eof()) fail();
        return static_cast<std::uint8_t>(value);
    }
    std::uint32_t u32() { return static_cast<std::uint32_t>(u8()) | (static_cast<std::uint32_t>(u8()) << 8) | (static_cast<std::uint32_t>(u8()) << 16) | (static_cast<std::uint32_t>(u8()) << 24); }
    std::uint64_t u64() {
        std::uint64_t value = 0;
        for (int i = 0; i < 8; ++i) value |= static_cast<std::uint64_t>(u8()) << (i * 8);
        return value;
    }
    std::string string() {
        const auto size = u64();
        if (size > max_string) throw GraphError("SPL artifact string is too large");
        std::string value(size, '\0');
        stream_.read(value.data(), static_cast<std::streamsize>(size));
        if (!stream_) fail();
        return value;
    }
    std::uint64_t count() {
        const auto value = u64();
        if (value > max_count) throw GraphError("SPL artifact collection is too large");
        return value;
    }

private:
    [[noreturn]] void fail() const { throw GraphError("malformed or truncated SPL pipeline artifact"); }
    std::istream& stream_;
};

void write_type(Writer& writer, Type type) { writer.u8(static_cast<std::uint8_t>(type)); }
Type read_type(Reader& reader) {
    const auto value = reader.u8();
    if (value > static_cast<std::uint8_t>(Type::String)) throw GraphError("unknown SPL type in artifact");
    return static_cast<Type>(value);
}

void write_value(Writer& writer, const Value& value) {
    write_type(writer, value.type());
    switch (value.type()) {
        case Type::Int64: writer.u64(static_cast<std::uint64_t>(value.get<std::int64_t>())); break;
        case Type::Float64: {
            static_assert(sizeof(double) == sizeof(std::uint64_t));
            std::uint64_t bits;
            std::memcpy(&bits, &value.get<double>(), sizeof(bits));
            writer.u64(bits);
            break;
        }
        case Type::Bool: writer.u8(value.get<bool>() ? 1 : 0); break;
        case Type::String: writer.string(value.get<std::string>()); break;
    }
}

Value read_value(Reader& reader) {
    const auto type = read_type(reader);
    switch (type) {
        case Type::Int64: return Value{static_cast<std::int64_t>(reader.u64())};
        case Type::Float64: {
            const auto bits = reader.u64();
            double value;
            std::memcpy(&value, &bits, sizeof(value));
            return Value{value};
        }
        case Type::Bool: {
            const auto value = reader.u8();
            if (value > 1) throw GraphError("invalid Bool value in artifact");
            return Value{value == 1};
        }
        case Type::String: return Value{reader.string()};
    }
    throw GraphError("unknown SPL value in artifact");
}

void write_binding(Writer& writer, const Binding& binding) {
    writer.u8(static_cast<std::uint8_t>(binding.kind));
    if (binding.kind == Binding::Kind::Literal) write_value(writer, binding.literal);
    else if (binding.kind == Binding::Kind::PipelineInput) writer.string(binding.name);
    else {
        writer.string(binding.node_id);
        writer.string(binding.port_name);
    }
}

Binding read_binding(Reader& reader) {
    const auto kind = reader.u8();
    if (kind > static_cast<std::uint8_t>(Binding::Kind::NodeOutput)) throw GraphError("unknown binding in artifact");
    if (kind == static_cast<std::uint8_t>(Binding::Kind::Literal)) return Binding::literal_value(read_value(reader));
    if (kind == static_cast<std::uint8_t>(Binding::Kind::PipelineInput)) return Binding::pipeline_input(reader.string());
    const auto node_id = reader.string();
    const auto port_name = reader.string();
    return Binding::node_output(node_id, port_name);
}

void write_ports(Writer& writer, const std::vector<Port>& ports) {
    writer.u64(ports.size());
    for (const auto& port : ports) {
        writer.string(port.name);
        writer.u8(static_cast<std::uint8_t>(port.direction));
        write_type(writer, port.type);
        writer.u8(port.required ? 1 : 0);
    }
}

std::vector<Port> read_ports(Reader& reader) {
    std::vector<Port> ports;
    for (std::uint64_t i = 0, size = reader.count(); i < size; ++i) {
        const auto name = reader.string();
        const auto direction = reader.u8();
        if (direction > static_cast<std::uint8_t>(PortDirection::Output)) throw GraphError("unknown port direction in artifact");
        const auto type = read_type(reader);
        const auto required = reader.u8();
        if (required > 1) throw GraphError("invalid required flag in artifact");
        ports.push_back(Port{name, static_cast<PortDirection>(direction), type, required == 1});
    }
    return ports;
}

} // namespace

void write_pipeline_artifact(const Graph& graph, const std::filesystem::path& path) {
    std::function<void(Writer&, const Graph&)> write_graph = [&](Writer& writer, const Graph& current) {
        current.validate();
        writer.string(current.pipeline_name_);

        writer.u64(current.errors_.size());
        for (const auto& [id, definition] : current.errors_) {
            writer.string(id);
            writer.u64(definition.fields.size());
            for (const auto& field : definition.fields) { writer.string(field.name); write_type(writer, field.type); }
        }
        writer.u64(current.definitions_.size());
        for (const auto& [id, definition] : current.definitions_) {
            writer.string(id);
            write_ports(writer, definition.ports);
            writer.u64(definition.error_types.size());
            for (const auto& error : definition.error_types) writer.string(error);
            writer.u8(definition.pipeline_implementation ? 1 : 0);
            if (definition.pipeline_implementation) write_graph(writer, *definition.pipeline_implementation);
            else {
                if (!definition.runtime_implementation) throw GraphError("cannot serialize a native in-memory node definition");
                writer.string(definition.runtime_implementation->runtime);
                writer.string(std::filesystem::absolute(definition.runtime_implementation->target).lexically_normal().string());
            }
        }
        writer.u64(current.inputs_.size());
        for (const auto& [name, type] : current.inputs_) { writer.string(name); write_type(writer, type); }
        writer.u64(current.instances_.size());
        for (const auto& [id, instance] : current.instances_) {
            writer.string(id);
            writer.string(instance.definition_id);
            writer.u64(instance.inputs.size());
            for (const auto& [name, binding] : instance.inputs) { writer.string(name); write_binding(writer, binding); }
        }
        writer.u64(current.outputs_.size());
        for (const auto& [name, output] : current.outputs_) {
            writer.string(name);
            writer.u8(output.type.has_value() ? 1 : 0);
            if (output.type) write_type(writer, *output.type);
            write_binding(writer, output.binding);
        }
    };

    std::ofstream file(path, std::ios::binary);
    if (!file) throw GraphError("cannot create SPL pipeline artifact: " + path.string());
    Writer writer(file);
    file.write("SPLB", 4);
    writer.u32(artifact_version);
    write_graph(writer, graph);
}

Graph read_pipeline_artifact(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw GraphError("cannot open SPL pipeline artifact: " + path.string());
    Reader reader(file);
    char magic[4];
    file.read(magic, 4);
    if (!file || std::string(magic, 4) != "SPLB") throw GraphError("invalid SPL pipeline artifact header");
    if (reader.u32() != artifact_version) throw GraphError("unsupported SPL pipeline artifact version");

    std::function<Graph()> read_graph = [&]() {
        Graph graph;
        graph.pipeline_name_ = reader.string();
        for (std::uint64_t i = 0, size = reader.count(); i < size; ++i) {
            ErrorDefinition definition{reader.string(), {}};
            for (std::uint64_t j = 0, fields = reader.count(); j < fields; ++j) {
                const auto name = reader.string();
                definition.fields.push_back(ErrorField{name, read_type(reader)});
            }
            graph.add_error_definition(std::move(definition));
        }
        for (std::uint64_t i = 0, size = reader.count(); i < size; ++i) {
            NodeDefinition definition;
            definition.id = reader.string();
            definition.ports = read_ports(reader);
            for (std::uint64_t j = 0, errors = reader.count(); j < errors; ++j) definition.error_types.push_back(reader.string());
            const auto kind = reader.u8();
            if (kind > 1) throw GraphError("unknown node implementation in artifact");
            if (kind == 1) definition.pipeline_implementation = std::make_shared<Graph>(read_graph());
            else definition.runtime_implementation = runtime::Implementation{reader.string(), reader.string()};
            graph.add_definition(std::move(definition));
        }
        for (std::uint64_t i = 0, size = reader.count(); i < size; ++i) {
            const auto name = reader.string();
            const auto type = read_type(reader);
            graph.add_input(name, type);
        }
        for (std::uint64_t i = 0, size = reader.count(); i < size; ++i) {
            const auto id = reader.string();
            const auto definition_id = reader.string();
            NodeInstance instance{id, definition_id, {}};
            for (std::uint64_t j = 0, inputs = reader.count(); j < inputs; ++j) {
                const auto name = reader.string();
                instance.inputs.emplace(name, read_binding(reader));
            }
            graph.add_instance(std::move(instance));
        }
        for (std::uint64_t i = 0, size = reader.count(); i < size; ++i) {
            const auto name = reader.string();
            const auto has_type = reader.u8();
            if (has_type > 1) throw GraphError("invalid output type flag in artifact");
            const auto type = has_type ? std::optional<Type>{read_type(reader)} : std::nullopt;
            const auto binding = read_binding(reader);
            if (type) graph.add_output(name, *type, binding); else graph.add_output(name, binding);
        }
        graph.validate();
        return graph;
    };
    return read_graph();
}

} // namespace spl
