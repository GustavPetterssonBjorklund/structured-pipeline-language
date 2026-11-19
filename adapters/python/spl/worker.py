"""Worker used by the packaged Python runtime adapter."""

import argparse
import importlib.util
import json
from pathlib import Path


def type_name(value):
    if isinstance(value, bool): return "Bool"
    if isinstance(value, int): return "Int64"
    if isinstance(value, float): return "Float64"
    if isinstance(value, str): return "String"
    raise TypeError(f"unsupported SPL output type: {type(value).__name__}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", required=True)
    parser.add_argument("--input-json", required=True)
    args = parser.parse_args()
    module_name, function_name = args.target.split("::", maxsplit=1)
    spec = importlib.util.spec_from_file_location("spl_runtime_node", Path(module_name))
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load Python node: {args.target}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    request = json.loads(args.input_json)
    inputs = {name: entry["value"] for name, entry in request["inputs"].items()}
    outputs = getattr(module, function_name)(inputs)
    print(json.dumps({"version": 1, "outputs": {
        name: {"type": type_name(value), "value": value}
        for name, value in outputs.items()
    }}, separators=(",", ":")))


if __name__ == "__main__":
    main()
