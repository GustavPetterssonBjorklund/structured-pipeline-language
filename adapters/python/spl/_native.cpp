#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <spl/Artifact.hpp>
#include <spl/Parser.hpp>
#include <spl/PreparedGraph.hpp>
#include <spl/runtime/PythonAdapter.hpp>
#include <spl/runtime/JavaScriptAdapter.hpp>
#include <spl/Observability.hpp>
#include <spl/logging/Logger.hpp>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <map>
#include <mutex>

namespace {

PyObject* native_error = nullptr;
std::mutex callback_mutex;
std::map<spl::SubscriptionId, PyObject*> python_log_callbacks;
PyObject* python_serial_callback = nullptr;
constexpr const char* pipeline_capsule_name = "spl.PreparedPipeline";

PyObject* log_record_dict(const spl::logging::LogRecord& record) {
    PyObject* result = PyDict_New();
    PyDict_SetItemString(result, "level", PyUnicode_FromString(std::string(spl::logging::log_level_name(record.level)).c_str()));
    PyDict_SetItemString(result, "module", PyUnicode_FromString(record.module.c_str()));
    PyDict_SetItemString(result, "message", PyUnicode_FromString(record.message.c_str()));
    PyDict_SetItemString(result, "timestamp", PyUnicode_FromString(record.timestamp.c_str()));
    PyObject* metadata = PyDict_New();
    for (const auto& [name, value] : record.metadata) PyDict_SetItemString(metadata, name.c_str(), PyUnicode_FromString(value.c_str()));
    PyDict_SetItemString(result, "metadata", metadata);
    Py_DECREF(metadata);
    return result;
}

void invoke_python_log(PyObject* callback, const spl::logging::LogRecord& record) {
    const auto gil = PyGILState_Ensure();
    PyObject* argument = log_record_dict(record);
    PyObject* result = PyObject_CallFunctionObjArgs(callback, argument, nullptr);
    Py_XDECREF(result);
    Py_DECREF(argument);
    if (PyErr_Occurred()) PyErr_Clear();
    PyGILState_Release(gil);
}

void invoke_python_console(PyObject* handle, std::string_view line) {
    const auto gil = PyGILState_Ensure();
    PyObject* write = PyObject_GetAttrString(handle, "write");
    if (!write || !PyCallable_Check(write)) {
        Py_XDECREF(write);
        PyGILState_Release(gil);
        return;
    }
    PyObject* argument = PyUnicode_FromStringAndSize(line.data(), static_cast<Py_ssize_t>(line.size()));
    PyObject* result = PyObject_CallFunctionObjArgs(write, argument, nullptr);
    Py_XDECREF(result);
    Py_DECREF(write);
    Py_DECREF(argument);
    PyObject* flush = PyObject_GetAttrString(handle, "flush");
    if (flush && PyCallable_Check(flush)) {
        result = PyObject_CallFunctionObjArgs(flush, nullptr);
        Py_XDECREF(result);
    }
    Py_XDECREF(flush);
    if (PyErr_Occurred()) PyErr_Clear();
    PyGILState_Release(gil);
}

PyObject* set_log_level(PyObject*, PyObject* args) {
    const char* value = nullptr;
    if (!PyArg_ParseTuple(args, "s:set_log_level", &value)) return nullptr;
    spl::logging::LogLevel level;
    if (!spl::logging::parse_log_level(value, level)) { PyErr_SetString(PyExc_ValueError, "unknown log level"); return nullptr; }
    spl::set_log_level(level);
    Py_RETURN_NONE;
}

PyObject* set_log_filter(PyObject*, PyObject* args) {
    const char* value = nullptr;
    if (!PyArg_ParseTuple(args, "s:set_log_filter", &value)) return nullptr;
    spl::set_log_filter(value);
    Py_RETURN_NONE;
}

PyObject* logs_subscribe(PyObject*, PyObject* args) {
    PyObject* callback = nullptr;
    if (!PyArg_ParseTuple(args, "O:logs_subscribe", &callback) || !PyCallable_Check(callback)) { PyErr_SetString(PyExc_TypeError, "callback must be callable"); return nullptr; }
    Py_INCREF(callback);
    const auto id = spl::logs_subscribe([callback](const spl::logging::LogRecord& record) { invoke_python_log(callback, record); });
    { std::lock_guard lock(callback_mutex); python_log_callbacks.emplace(id, callback); }
    return PyLong_FromUnsignedLongLong(id);
}

PyObject* logs_unsubscribe(PyObject*, PyObject* args) {
    unsigned long long id = 0;
    if (!PyArg_ParseTuple(args, "K:logs_unsubscribe", &id)) return nullptr;
    spl::logs_unsubscribe(id);
    std::lock_guard lock(callback_mutex);
    const auto found = python_log_callbacks.find(id);
    if (found != python_log_callbacks.end()) { Py_DECREF(found->second); python_log_callbacks.erase(found); }
    Py_RETURN_NONE;
}

PyObject* set_console_interface(PyObject*, PyObject* args) {
    PyObject* handle = nullptr;
    PyObject* color = Py_None;
    if (!PyArg_ParseTuple(args, "O|O:set_console_interface", &handle, &color)) return nullptr;
    if (handle != Py_None) {
        PyObject* write = PyObject_GetAttrString(handle, "write");
        const bool valid = write && PyCallable_Check(write);
        Py_XDECREF(write);
        if (!valid) { PyErr_SetString(PyExc_TypeError, "console handle must provide callable write(str)"); return nullptr; }
    }
    bool use_color = false;
    if (color != Py_None) {
        if (PyBool_Check(color)) use_color = color == Py_True;
        else if (handle != Py_None && PyUnicode_Check(color) && std::string(PyUnicode_AsUTF8(color)) == "auto") {
            PyObject* isatty = PyObject_GetAttrString(handle, "isatty");
            if (isatty && PyCallable_Check(isatty)) {
                PyObject* result = PyObject_CallFunctionObjArgs(isatty, nullptr);
                use_color = result && PyObject_IsTrue(result);
                Py_XDECREF(result);
            }
            Py_XDECREF(isatty);
        } else { PyErr_SetString(PyExc_TypeError, "color must be True, False, or 'auto'"); return nullptr; }
    }
    PyObject* old = nullptr;
    { std::lock_guard lock(callback_mutex); old = python_serial_callback; python_serial_callback = nullptr; if (handle != Py_None) { Py_INCREF(handle); python_serial_callback = handle; } }
    Py_XDECREF(old);
    if (handle == Py_None) spl::clear_console_handle();
    else {
        spl::set_console_color(use_color);
        spl::set_serial_interface([handle](std::string_view line) { invoke_python_console(handle, line); });
    }
    Py_RETURN_NONE;
}

struct NativePipeline {
    spl::PreparedGraph graph;
    spl::runtime::RuntimeRegistry runtimes;

    NativePipeline(spl::PreparedGraph prepared, std::string package_path)
        : graph(std::move(prepared)) {
        runtimes.register_adapter(std::make_unique<spl::runtime::PythonAdapter>(std::move(package_path)));
        runtimes.register_adapter(std::make_unique<spl::runtime::JavaScriptAdapter>());
    }
};

void destroy_pipeline(PyObject* capsule) {
    delete static_cast<NativePipeline*>(PyCapsule_GetPointer(capsule, pipeline_capsule_name));
}

std::string pipeline_name(PyObject* object) {
    if (object == Py_None) return {};
    if (!PyUnicode_Check(object)) throw std::invalid_argument("pipeline name must be a string or None");
    const auto value = PyUnicode_AsUTF8(object);
    if (!value) throw std::invalid_argument("pipeline name is not valid UTF-8");
    return value;
}

spl::Value to_value(PyObject* object, spl::Type expected, const std::string& name) {
    PyObject* type = PyObject_GetAttrString(object, "spl_type");
    if (!type) {
        PyErr_Clear();
        throw std::invalid_argument("input '" + name + "' must be an SPL value");
    }
    const auto type_text = PyUnicode_Check(type) ? PyUnicode_AsUTF8(type) : nullptr;
    const auto expected_text = spl::type_name(expected);
    if (!type_text || type_text != expected_text) {
        Py_DECREF(type);
        throw std::invalid_argument("input '" + name + "' has the wrong SPL type");
    }
    Py_DECREF(type);
    PyObject* raw = PyObject_GetAttrString(object, "value");
    if (!raw) {
        PyErr_Clear();
        throw std::invalid_argument("input '" + name + "' is not a valid SPL value");
    }
    try {
        spl::Value result = [&]() -> spl::Value {
            switch (expected) {
                case spl::Type::Int64: {
                    if (!PyLong_Check(raw) || PyBool_Check(raw)) throw std::invalid_argument("Int64 value is invalid");
                    const auto value = PyLong_AsLongLong(raw);
                    if (PyErr_Occurred()) { PyErr_Clear(); throw std::overflow_error("Int64 value is out of range"); }
                    return spl::Value{static_cast<std::int64_t>(value)};
                }
                case spl::Type::Float64:
                    if (!PyFloat_Check(raw)) throw std::invalid_argument("Float64 value is invalid");
                    return spl::Value{PyFloat_AsDouble(raw)};
                case spl::Type::Bool:
                    if (!PyBool_Check(raw)) throw std::invalid_argument("Bool value is invalid");
                    return spl::Value{raw == Py_True};
                case spl::Type::String:
                    if (!PyUnicode_Check(raw)) throw std::invalid_argument("String value is invalid");
                    return spl::Value{std::string(PyUnicode_AsUTF8(raw))};
            }
            throw std::invalid_argument("unsupported SPL type");
        }();
        Py_DECREF(raw);
        return result;
    } catch (...) {
        Py_DECREF(raw);
        throw;
    }
}

PyObject* from_value(const spl::Value& value) {
    if (value.is(spl::Type::Int64)) return PyLong_FromLongLong(value.get<std::int64_t>());
    if (value.is(spl::Type::Float64)) return PyFloat_FromDouble(value.get<double>());
    if (value.is(spl::Type::Bool)) return PyBool_FromLong(value.get<bool>() ? 1 : 0);
    return PyUnicode_FromStringAndSize(value.get<std::string>().data(), static_cast<Py_ssize_t>(value.get<std::string>().size()));
}

PyObject* type_map(const std::map<std::string, spl::Type>& values) {
    PyObject* result = PyDict_New();
    for (const auto& [name, type] : values) {
        PyObject* value = PyUnicode_FromString(std::string(spl::type_name(type)).c_str());
        PyDict_SetItemString(result, name.c_str(), value);
        Py_DECREF(value);
    }
    return result;
}

PyObject* pipeline_metadata(const spl::PreparedGraph& graph) {
    PyObject* result = PyDict_New();
    PyObject* inputs = type_map(graph.input_types());
    PyObject* outputs = type_map(graph.output_types());
    PyDict_SetItemString(result, "inputs", inputs);
    PyDict_SetItemString(result, "outputs", outputs);
    Py_DECREF(inputs);
    Py_DECREF(outputs);
    return result;
}

PyObject* make_pipeline(spl::PreparedGraph graph, std::string package_path) {
    auto* native = new NativePipeline(std::move(graph), std::move(package_path));
    PyObject* capsule = PyCapsule_New(native, pipeline_capsule_name, destroy_pipeline);
    if (!capsule) {
        delete native;
        return nullptr;
    }
    PyObject* metadata = pipeline_metadata(native->graph);
    PyObject* result = PyTuple_New(2);
    PyTuple_SET_ITEM(result, 0, capsule);
    PyTuple_SET_ITEM(result, 1, metadata);
    return result;
}

PyObject* pipeline_info(PyObject*, PyObject* args) {
    const char* path = nullptr;
    if (!PyArg_ParseTuple(args, "s:pipeline_info", &path)) return nullptr;
    try {
        return pipeline_metadata(spl::read_pipeline_artifact(path).prepare());
    } catch (const std::exception& error) {
        PyErr_SetString(native_error, error.what());
        return nullptr;
    }
}

PyObject* build_pipeline(PyObject*, PyObject* args) {
    const char* source = nullptr;
    PyObject* name = nullptr;
    const char* artifact = nullptr;
    const char* package_path = nullptr;
    if (!PyArg_ParseTuple(args, "sOss:build_pipeline", &source, &name, &artifact, &package_path)) return nullptr;
    try {
        const auto graph = spl::parse_pipeline_file(source, pipeline_name(name));
        spl::write_pipeline_artifact(graph, artifact);
        return make_pipeline(graph.prepare(), package_path);
    } catch (const std::exception& error) {
        PyErr_SetString(native_error, error.what());
        return nullptr;
    }
}

PyObject* load_pipeline(PyObject*, PyObject* args) {
    const char* artifact = nullptr;
    const char* package_path = nullptr;
    if (!PyArg_ParseTuple(args, "ss:load_pipeline", &artifact, &package_path)) return nullptr;
    try {
        return make_pipeline(spl::read_pipeline_artifact(artifact).prepare(), package_path);
    } catch (const std::exception& error) {
        PyErr_SetString(native_error, error.what());
        return nullptr;
    }
}

PyObject* run_pipeline(PyObject*, PyObject* args) {
    PyObject* capsule = nullptr;
    PyObject* input_dict = nullptr;
    const char* observability = "off";
    if (!PyArg_ParseTuple(args, "OO|s:run_pipeline", &capsule, &input_dict, &observability)) return nullptr;
    if (!PyDict_Check(input_dict)) {
        PyErr_SetString(PyExc_TypeError, "inputs must be a dict");
        return nullptr;
    }
    try {
        auto* pipeline = static_cast<NativePipeline*>(PyCapsule_GetPointer(capsule, pipeline_capsule_name));
        if (!pipeline) return nullptr;
        spl::Values inputs;
        for (const auto& [input_name, type] : pipeline->graph.input_types()) {
            PyObject* item = PyDict_GetItemString(input_dict, input_name.c_str());
            if (!item) throw std::invalid_argument("missing pipeline input: " + input_name);
            inputs.emplace(input_name, to_value(item, type, input_name));
        }
        if (PyDict_Size(input_dict) != static_cast<Py_ssize_t>(pipeline->graph.input_types().size())) {
            PyObject *key, *value;
            Py_ssize_t position = 0;
            while (PyDict_Next(input_dict, &position, &key, &value)) {
                const auto key_text = PyUnicode_Check(key) ? PyUnicode_AsUTF8(key) : nullptr;
                if (!key_text || !pipeline->graph.input_types().contains(key_text)) {
                    throw std::invalid_argument("unknown pipeline input");
                }
            }
        }
        spl::trace::ExecutionOptions options;
        if (std::string(observability) == "summary") options.observability.mode = spl::trace::ObservabilityMode::Summary;
        else if (std::string(observability) == "trace") options.observability.mode = spl::trace::ObservabilityMode::Trace;
        else if (std::string(observability) == "debug") {
            options.observability.mode = spl::trace::ObservabilityMode::Debug;
            options.observability.capture_inputs = true;
            options.observability.capture_outputs = true;
        } else if (std::string(observability) != "off") {
            throw std::invalid_argument("observability must be off, summary, trace, or debug");
        }
        const auto result = pipeline->graph.execute(inputs, &pipeline->runtimes, options);
        if (!result.succeeded()) throw std::runtime_error(result.error->to_string());
        PyObject* outputs = PyDict_New();
        for (const auto& [name_text, value] : result.outputs) {
            PyObject* entry = PyTuple_New(2);
            PyTuple_SET_ITEM(entry, 0, PyUnicode_FromString(std::string(spl::type_name(value.type())).c_str()));
            PyTuple_SET_ITEM(entry, 1, from_value(value));
            PyDict_SetItemString(outputs, name_text.c_str(), entry);
            Py_DECREF(entry);
        }
        return outputs;
    } catch (const std::exception& error) {
        PyErr_SetString(native_error, error.what());
        return nullptr;
    }
}

PyMethodDef methods[] = {
    {"build_pipeline", build_pipeline, METH_VARARGS, "Compile an SPL source file into an artifact."},
    {"load_pipeline", load_pipeline, METH_VARARGS, "Load and prepare a compiled SPL artifact."},
    {"pipeline_info", pipeline_info, METH_VARARGS, "Inspect pipeline input and output types."},
    {"run_pipeline", run_pipeline, METH_VARARGS, "Execute a typed SPL pipeline."},
    {"set_log_level", set_log_level, METH_VARARGS, "Set the global SPL log level."},
    {"set_log_filter", set_log_filter, METH_VARARGS, "Set the global SPL module filter."},
    {"logs_subscribe", logs_subscribe, METH_VARARGS, "Subscribe to structured SPL logs."},
    {"logs_unsubscribe", logs_unsubscribe, METH_VARARGS, "Remove a log subscription."},
    {"set_console_interface", set_console_interface, METH_VARARGS, "Set a formatted console handle."},
    {"set_serial_interface", set_console_interface, METH_VARARGS, "Set a formatted console handle."},
    {nullptr, nullptr, 0, nullptr},
};

PyModuleDef module = {PyModuleDef_HEAD_INIT, "spl._native", nullptr, -1, methods};

} // namespace

PyMODINIT_FUNC PyInit__native() {
    PyObject* result = PyModule_Create(&module);
    if (!result) return nullptr;
    native_error = PyErr_NewException("spl._native.Error", nullptr, nullptr);
    Py_INCREF(native_error);
    PyModule_AddObject(result, "Error", native_error);
    return result;
}
