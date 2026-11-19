# Structured Pipeline Language

SPL is a typed graph language for composing computational nodes across
multiple runtimes. The canonical semantic representation is SPL-IR.

## Repository layout

```text
include/spl/       Public SPL core and runtime interfaces
src/               C++ library implementation
adapters/          Language-specific package and binding adapters
tests/             C++ tests
runtimes/          Runtime-specific workers and adapters
examples/          SPL programs and example node implementations
editors/vscode/    VS Code language support
docs/              Design and implementation notes
```

## Development

Enter the Nix development shell:

```bash
nix-shell
```

Then run the full C++ verification flow:

```bash
just
```

The CTest suite is split by subsystem: value, graph, typed-error, parser, and
Python-runtime tests, plus the CLI example smoke test. Run one group with, for
example, `ctest --test-dir build -R spl_parser --output-on-failure`.

Run the current Python-backed example with:

```bash
just example
```

The current example is executed by the C++ SPL engine and dispatches its nodes
through the embedded Python adapter. Python nodes import `spl` and receive
typed SPL values directly; the active path does not serialize node inputs as
JSON.

SPL types remain explicit at the Python boundary:

```python
import spl

pipeline = spl.load_pipeline("examples/pipeline.spl:Calculation")
outputs = pipeline.run({"x": spl.Int64(3), "y": spl.Int64(4)})
assert outputs["result"] == spl.Int64(14)
```

For a reproducible environment, prefer `nix develop` with the included
`flake.nix`; a regular system with CMake, a C++20 compiler, Python 3, and Just
is also supported.
