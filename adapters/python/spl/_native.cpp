#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <spl/Parser.hpp>
#include <spl/runtime/PythonAdapter.hpp>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

PyObject* native_error = nullptr;

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

PyObject* pipeline_info(PyObject*, PyObject* args) {
    const char* path = nullptr;
    PyObject* name = nullptr;
    if (!PyArg_ParseTuple(args, "sO:pipeline_info", &path, &name)) return nullptr;
    try {
        const auto graph = spl::parse_pipeline_file(path, pipeline_name(name));
        PyObject* result = PyDict_New();
        PyObject* inputs = type_map(graph.input_types());
        PyObject* outputs = type_map(graph.output_types());
        PyDict_SetItemString(result, "inputs", inputs);
        PyDict_SetItemString(result, "outputs", outputs);
        Py_DECREF(inputs);
        Py_DECREF(outputs);
        return result;
    } catch (const std::exception& error) {
        PyErr_SetString(native_error, error.what());
        return nullptr;
    }
}

PyObject* run_pipeline(PyObject*, PyObject* args) {
    const char* path = nullptr;
    PyObject* name = nullptr;
    const char* package_path = nullptr;
    PyObject* input_dict = nullptr;
    if (!PyArg_ParseTuple(args, "sOsO:run_pipeline", &path, &name, &package_path, &input_dict)) return nullptr;
    if (!PyDict_Check(input_dict)) {
        PyErr_SetString(PyExc_TypeError, "inputs must be a dict");
        return nullptr;
    }
    try {
        const auto graph = spl::parse_pipeline_file(path, pipeline_name(name));
        spl::Values inputs;
        for (const auto& [input_name, type] : graph.input_types()) {
            PyObject* item = PyDict_GetItemString(input_dict, input_name.c_str());
            if (!item) throw std::invalid_argument("missing pipeline input: " + input_name);
            inputs.emplace(input_name, to_value(item, type, input_name));
        }
        if (PyDict_Size(input_dict) != static_cast<Py_ssize_t>(graph.input_types().size())) {
            PyObject *key, *value;
            Py_ssize_t position = 0;
            while (PyDict_Next(input_dict, &position, &key, &value)) {
                const auto key_text = PyUnicode_Check(key) ? PyUnicode_AsUTF8(key) : nullptr;
                if (!key_text || !graph.input_types().contains(key_text)) {
                    throw std::invalid_argument("unknown pipeline input");
                }
            }
        }
        spl::runtime::RuntimeRegistry runtimes;
        runtimes.register_adapter(std::make_unique<spl::runtime::PythonAdapter>(package_path));
        const auto result = graph.execute(inputs, &runtimes);
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
    {"pipeline_info", pipeline_info, METH_VARARGS, "Inspect pipeline input and output types."},
    {"run_pipeline", run_pipeline, METH_VARARGS, "Execute a typed SPL pipeline."},
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
