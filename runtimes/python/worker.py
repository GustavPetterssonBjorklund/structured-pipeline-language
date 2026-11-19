"""Minimal Python runtime invoked by the C++ SPL engine."""

import argparse
import importlib.util
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", required=True)
    parser.add_argument("--input", action="append", default=[])
    arguments = parser.parse_args()

    module_name, function_name = arguments.target.split("::", maxsplit=1)
    module_path = Path(module_name)
    spec = importlib.util.spec_from_file_location("spl_runtime_node", module_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load Python node: {arguments.target}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    function = getattr(module, function_name)
    inputs = {item.split("=", maxsplit=1)[0]: int(item.split("=", maxsplit=1)[1]) for item in arguments.input}
    outputs = function(inputs)
    for name, value in outputs.items():
        print(f"{name}={value}")


if __name__ == "__main__":
    main()
