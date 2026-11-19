"""Minimal Python runtime invoked by the C++ SPL engine."""

import argparse
import importlib.util
import json
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", required=True)
    parser.add_argument("--input", action="append", default=[])
    parser.add_argument("--input-json")
    arguments = parser.parse_args()

    module_name, function_name = arguments.target.split("::", maxsplit=1)
    module_path = Path(module_name)
    spec = importlib.util.spec_from_file_location("spl_runtime_node", module_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load Python node: {arguments.target}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    function = getattr(module, function_name)
    if arguments.input_json:
        request = json.loads(arguments.input_json)
        inputs = {name: entry["value"] for name, entry in request["inputs"].items()}
    else:
        inputs = {item.split("=", maxsplit=1)[0]: int(item.split("=", maxsplit=1)[1]) for item in arguments.input}
    outputs = function(inputs)
    if arguments.input_json:
        response = {"version": 1, "outputs": {name: {"type": type_name(value), "value": value} for name, value in outputs.items()}}
        print(json.dumps(response, separators=(",", ":")))
    else:
        for name, value in outputs.items():
            print(f"{name}={value}")


def type_name(value: object) -> str:
    if isinstance(value, bool):
        return "Bool"
    if isinstance(value, int):
        return "Int64"
    if isinstance(value, float):
        return "Float64"
    if isinstance(value, str):
        return "String"
    raise TypeError(f"unsupported SPL output type: {type(value).__name__}")


if __name__ == "__main__":
    main()
