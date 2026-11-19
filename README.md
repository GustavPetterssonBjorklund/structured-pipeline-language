# Structured Pipeline Language

SPL is a typed graph language for composing computational nodes across
multiple runtimes. The canonical semantic representation is SPL-IR.

## Repository layout

```text
include/spl/       Public SPL core and runtime interfaces
src/               C++ library implementation
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

Run the current Python-backed example with:

```bash
just example
```

The current example is executed by the C++ SPL engine and dispatches its nodes
through the Python runtime worker.
