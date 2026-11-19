#include <spl/runtime/PythonAdapter.hpp>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <stdexcept>
#include <string>

namespace spl::runtime {
namespace {

std::string python_error(const std::string& prefix) {
    PyObject *type = nullptr, *value = nullptr, *traceback = nullptr;
    PyErr_Fetch(&type, &value, &traceback);
    PyErr_NormalizeException(&type, &value, &traceback);
    std::string message = prefix;
    if (value) {
        PyObject* text = PyObject_Str(value);
        if (text) {
            const auto* utf8 = PyUnicode_AsUTF8(text);
            if (utf8) message += ": " + std::string(utf8);
            Py_DECREF(text);
        }
    }
    Py_XDECREF(type);
    Py_XDECREF(value);
    Py_XDECREF(traceback);
    return message;
}

[[noreturn]] void throw_python_error(const std::string& prefix) {
    throw std::runtime_error(python_error(prefix));
}

void require_python_call(PyObject* result, const std::string& context) {
    if (!result) throw_python_error(context);
}

PyObject* raw_python_value(const Value& value) {
    if (value.is(Type::Int64)) return PyLong_FromLongLong(value.get<std::int64_t>());
    if (value.is(Type::Float64)) return PyFloat_FromDouble(value.get<double>());
    if (value.is(Type::Bool)) return PyBool_FromLong(value.get<bool>() ? 1 : 0);
    return PyUnicode_FromStringAndSize(value.get<std::string>().data(),
                                       static_cast<Py_ssize_t>(value.get<std::string>().size()));
}

PyObject* spl_value(const Value& value) {
    PyObject* module = PyImport_ImportModule("spl");
    require_python_call(module, "cannot import spl");
    PyObject* type = PyObject_GetAttrString(module, std::string(type_name(value.type())).c_str());
    Py_DECREF(module);
    require_python_call(type, "spl is missing a value type");
    PyObject* raw = raw_python_value(value);
    require_python_call(raw, "cannot create Python value");
    PyObject* result = PyObject_CallFunctionObjArgs(type, raw, nullptr);
    Py_DECREF(raw);
    Py_DECREF(type);
    require_python_call(result, "cannot construct SPL value");
    return result;
}

Value cpp_value(PyObject* object, Type expected, const std::string& port) {
    PyObject* type = PyObject_GetAttrString(object, "spl_type");
    if (!type) {
        PyErr_Clear();
        throw std::runtime_error("Python output '" + port + "' is not an SPL value");
    }
    const auto* actual = PyUnicode_Check(type) ? PyUnicode_AsUTF8(type) : nullptr;
    const auto expected_name = type_name(expected);
    if (!actual || actual != expected_name) {
        Py_DECREF(type);
        throw std::runtime_error("Python output '" + port + "' has the wrong SPL type");
    }
    Py_DECREF(type);
    PyObject* raw = PyObject_GetAttrString(object, "value");
    if (!raw) {
        PyErr_Clear();
        throw std::runtime_error("Python output '" + port + "' has no value");
    }
    try {
        switch (expected) {
            case Type::Int64: {
                if (!PyLong_Check(raw) || PyBool_Check(raw)) throw std::runtime_error("invalid Int64 output");
                const auto result = PyLong_AsLongLong(raw);
                if (PyErr_Occurred()) { PyErr_Clear(); throw std::runtime_error("Int64 output is out of range"); }
                Py_DECREF(raw);
                return Value{static_cast<std::int64_t>(result)};
            }
            case Type::Float64:
                if (!PyFloat_Check(raw)) throw std::runtime_error("invalid Float64 output");
                {
                    const auto result = PyFloat_AsDouble(raw);
                    Py_DECREF(raw);
                    return Value{result};
                }
            case Type::Bool:
                if (!PyBool_Check(raw)) throw std::runtime_error("invalid Bool output");
                {
                    const auto result = raw == Py_True;
                    Py_DECREF(raw);
                    return Value{result};
                }
            case Type::String:
                if (!PyUnicode_Check(raw)) throw std::runtime_error("invalid String output");
                {
                    const auto* result = PyUnicode_AsUTF8(raw);
                    if (!result) throw std::runtime_error("invalid UTF-8 String output");
                    const std::string copy(result);
                    Py_DECREF(raw);
                    return Value{copy};
                }
        }
    } catch (...) {
        Py_DECREF(raw);
        throw;
    }
    Py_DECREF(raw);
    throw std::runtime_error("unsupported SPL output type");
}

PyObject* load_function(const std::string& target) {
    const auto separator = target.find("::");
    if (separator == std::string::npos) throw std::runtime_error("Python target must be file.py::function");
    const auto path = target.substr(0, separator);
    const auto function_name = target.substr(separator + 2);
    PyObject* util = PyImport_ImportModule("importlib.util");
    require_python_call(util, "cannot import importlib.util");
    PyObject* spec = PyObject_CallMethod(util, "spec_from_file_location", "ss", "spl_runtime_node", path.c_str());
    Py_DECREF(util);
    require_python_call(spec, "cannot create Python module spec");
    PyObject* module_from_spec = PyImport_ImportModule("importlib.util");
    require_python_call(module_from_spec, "cannot import importlib.util");
    PyObject* module = PyObject_CallMethod(module_from_spec, "module_from_spec", "O", spec);
    Py_DECREF(module_from_spec);
    require_python_call(module, "cannot create Python module");
    PyObject* loader = PyObject_GetAttrString(spec, "loader");
    Py_DECREF(spec);
    require_python_call(loader, "Python module has no loader");
    PyObject* executed = PyObject_CallMethod(loader, "exec_module", "O", module);
    Py_DECREF(loader);
    require_python_call(executed, "cannot execute Python module");
    Py_DECREF(executed);
    PyObject* function = PyObject_GetAttrString(module, function_name.c_str());
    Py_DECREF(module);
    require_python_call(function, "Python module has no target function");
    if (!PyCallable_Check(function)) {
        Py_DECREF(function);
        throw std::runtime_error("Python target is not callable");
    }
    return function;
}

void add_package_path(const std::string& package_path) {
    PyObject* sys = PyImport_ImportModule("sys");
    require_python_call(sys, "cannot import sys");
    PyObject* paths = PyObject_GetAttrString(sys, "path");
    Py_DECREF(sys);
    require_python_call(paths, "cannot access sys.path");
    PyObject* path = PyUnicode_FromString(package_path.c_str());
    require_python_call(path, "cannot create Python package path");
    const int result = PyList_Insert(paths, 0, path);
    Py_DECREF(path);
    Py_DECREF(paths);
    if (result != 0) throw_python_error("cannot update sys.path");
}

} // namespace

PythonAdapter::PythonAdapter(std::string python_package_path)
    : python_package_path_(std::move(python_package_path)) {
    if (!Py_IsInitialized()) Py_Initialize();
}

PythonAdapter::~PythonAdapter() = default;

std::string PythonAdapter::id() const { return "python"; }

NodeResult PythonAdapter::invoke(
    const Implementation& implementation,
    const Values& inputs,
    const ExecutionContext& context) {
    const auto gil = PyGILState_Ensure();
    try {
        add_package_path(python_package_path_);
        PyObject* function = load_function(implementation.target);
        PyObject* kwargs = PyDict_New();
        require_python_call(kwargs, "cannot create Python arguments");
        for (const auto& [name, value] : inputs) {
            PyObject* argument = spl_value(value);
            if (PyDict_SetItemString(kwargs, name.c_str(), argument) != 0) {
                Py_DECREF(argument);
                Py_DECREF(kwargs);
                Py_DECREF(function);
                throw_python_error("cannot build Python arguments");
            }
            Py_DECREF(argument);
        }
        PyObject* positional = PyTuple_New(0);
        require_python_call(positional, "cannot create Python positional arguments");
        PyObject* result = PyObject_Call(function, positional, kwargs);
        Py_DECREF(positional);
        Py_DECREF(kwargs);
        Py_DECREF(function);
        require_python_call(result, "Python node failed");

        Values outputs;
        if (context.outputs.size() == 1) {
            const auto& output = context.outputs.front();
            PyObject* values = PyObject_GetAttrString(result, "values");
            if (values) {
                if (!PyDict_Check(values)) {
                    Py_DECREF(values);
                    Py_DECREF(result);
                    throw std::runtime_error("spl.Outputs.values must be a dict");
                }
                PyObject* item = PyDict_GetItemString(values, output.name.c_str());
                if (!item) {
                    Py_DECREF(values);
                    Py_DECREF(result);
                    throw std::runtime_error("Python output is missing '" + output.name + "'");
                }
                outputs.emplace(output.name, cpp_value(item, output.type, output.name));
                Py_DECREF(values);
            } else {
                PyErr_Clear();
                outputs.emplace(output.name, cpp_value(result, output.type, output.name));
            }
        } else {
            PyObject* values = PyObject_GetAttrString(result, "values");
            if (!values || !PyDict_Check(values)) {
                Py_XDECREF(values);
                Py_DECREF(result);
                throw std::runtime_error("multiple Python outputs require spl.Outputs(...)");
            }
            for (const auto& output : context.outputs) {
                PyObject* item = PyDict_GetItemString(values, output.name.c_str());
                if (!item) {
                    Py_DECREF(values);
                    Py_DECREF(result);
                    throw std::runtime_error("Python output is missing '" + output.name + "'");
                }
                outputs.emplace(output.name, cpp_value(item, output.type, output.name));
            }
            Py_DECREF(values);
        }
        Py_DECREF(result);
        PyGILState_Release(gil);
        return outputs;
    } catch (...) {
        PyGILState_Release(gil);
        throw;
    }
}

} // namespace spl::runtime
