"""Typed Python interface for the Structured Pipeline Language."""

from pathlib import Path

try:
    from . import _native
except ImportError:  # The embedded C++ adapter can use the pure Python types.
    _native = None


class SPLException(Exception):
    """An error reported by the SPL parser, validator, or runtime."""


class SPLValue:
    """Base class for values crossing the SPL/Python boundary."""

    spl_type = None

    def __init__(self, value):
        self._spl_value = self._validate(value)

    @classmethod
    def _validate(cls, value):
        return value

    @property
    def value(self):
        return self._spl_value

    def __repr__(self):
        return f"{type(self).__name__}({self._spl_value!r})"

    def __eq__(self, other):
        return type(self) is type(other) and self.value == other.value

    def __hash__(self):
        return hash((type(self), self.value))

    def _binary(self, other, operation, result_type):
        if not isinstance(other, SPLValue):
            return NotImplemented
        if type(self) is not type(other):
            raise TypeError(f"cannot combine {self.spl_type} and {other.spl_type}")
        return result_type(operation(self.value, other.value))


class Int64(SPLValue):
    spl_type = "Int64"

    @classmethod
    def _validate(cls, value):
        if isinstance(value, bool) or not isinstance(value, int):
            raise TypeError("Int64 requires a Python int")
        if value < -(2**63) or value > 2**63 - 1:
            raise OverflowError("Int64 value is outside the signed 64-bit range")
        return value

    def __add__(self, other):
        return self._binary(other, lambda left, right: left + right, Int64)

    def __mul__(self, other):
        return self._binary(other, lambda left, right: left * right, Int64)


class Float64(SPLValue):
    spl_type = "Float64"

    @classmethod
    def _validate(cls, value):
        if isinstance(value, bool) or not isinstance(value, (int, float)):
            raise TypeError("Float64 requires a Python float or int")
        return float(value)

    def __add__(self, other):
        return self._binary(other, lambda left, right: left + right, Float64)

    def __mul__(self, other):
        return self._binary(other, lambda left, right: left * right, Float64)


class Bool(SPLValue):
    spl_type = "Bool"

    @classmethod
    def _validate(cls, value):
        if not isinstance(value, bool):
            raise TypeError("Bool requires a Python bool")
        return value


class String(SPLValue):
    spl_type = "String"

    @classmethod
    def _validate(cls, value):
        if not isinstance(value, str):
            raise TypeError("String requires a Python str")
        return value


_TYPES = {value.spl_type: value for value in (Int64, Float64, Bool, String)}


class Outputs:
    """Named typed outputs returned by a multi-output SPL node."""

    def __init__(self, **values):
        if any(not isinstance(value, SPLValue) for value in values.values()):
            raise TypeError("spl.Outputs values must be SPL values")
        self.values = dict(values)


def _split_source_spec(spec):
    path = Path(spec)
    if not isinstance(spec, str):
        return path, None
    candidate, separator, name = spec.rpartition(":")
    if separator and candidate and Path(candidate).exists():
        return Path(candidate), name or None
    return path, None


def _wrap(type_name, value):
    return _TYPES[type_name](value)


class Pipeline:
    def __init__(self, path):
        self.path = Path(path)
        if _native is None:
            raise SPLException("the native SPL Python extension is not installed")
        try:
            info = _native.pipeline_info(str(self.path))
        except _native.Error as error:
            raise SPLException(str(error)) from None
        self.inputs = {name: _TYPES[type_name] for name, type_name in info["inputs"].items()}
        self.outputs = {name: _TYPES[type_name] for name, type_name in info["outputs"].items()}

    def run(self, inputs):
        if not isinstance(inputs, dict):
            raise TypeError("pipeline inputs must be a dict of SPL values")
        for name, value_type in self.inputs.items():
            if name not in inputs:
                raise SPLException(f"missing pipeline input: {name}")
            if type(inputs[name]) is not value_type:
                raise TypeError(f"input '{name}' requires spl.{value_type.__name__}")
        if set(inputs) != set(self.inputs):
            unknown = sorted(set(inputs) - set(self.inputs))
            raise SPLException(f"unknown pipeline input: {unknown[0]}")
        try:
            raw = _native.run_pipeline(
                str(self.path), str(Path(__file__).parent.parent), inputs,
            )
        except _native.Error as error:
            raise SPLException(str(error)) from None
        return {name: _wrap(type_name, value) for name, (type_name, value) in raw.items()}


def build_pipeline(spec, output=None):
    path, name = _split_source_spec(spec)
    if _native is None:
        raise SPLException("the native SPL Python extension is not installed")
    if output is None:
        filename = f"{path.stem}.{name}.splb" if name else f"{path.stem}.splb"
        output = path.with_name(filename)
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    try:
        _native.build_pipeline(str(path), name, str(output))
    except _native.Error as error:
        raise SPLException(str(error)) from None
    return Pipeline(output)


def load_pipeline(artifact):
    artifact = Path(artifact)
    if artifact.suffix != ".splb":
        raise SPLException("run-time loading requires a compiled .splb artifact; use build_pipeline first")
    return Pipeline(artifact)


def run_pipeline(artifact, inputs):
    return load_pipeline(artifact).run(inputs)


buildPipeline = build_pipeline
loadPipeline = load_pipeline
runPipeline = run_pipeline


__all__ = [
    "Bool", "Float64", "Int64", "Outputs", "Pipeline", "SPLException", "SPLValue",
    "String", "build_pipeline", "buildPipeline", "load_pipeline", "loadPipeline",
    "run_pipeline", "runPipeline",
]
