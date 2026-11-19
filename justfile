set dotenv-load := false

# Run the complete local verification flow.
default: check

# Configure, build, and test the project.
check: build test

# Configure and compile the C++ project.
build:
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
    cmake --build build

# Run the CTest suite. The build recipe runs first when using `just check`.
test: build
    ctest --test-dir build --output-on-failure

# Create the local Python environment and install SPL only when its inputs changed.
venv:
    test -x .venv/bin/python || python3 -m venv --upgrade-deps .venv
    if test ! -f .venv/.spl-installed || test setup.py -nt .venv/.spl-installed || test pyproject.toml -nt .venv/.spl-installed || find adapters/python/spl src include -type f \( -name '*.cpp' -o -name '*.hpp' \) -newer .venv/.spl-installed -print -quit | grep -q .; then .venv/bin/python -m pip install -e . && touch .venv/.spl-installed; fi

# Compile the example source into a run-time artifact.
build-example: venv
    if test ! -f examples/pipeline.Calculation.splb || test examples/pipeline.spl -nt examples/pipeline.Calculation.splb; then .venv/bin/python -c 'import spl; spl.build_pipeline("examples/pipeline.spl:Calculation")'; fi

# Execute the compiled SPL example through the typed Python package.
example: build-example
    .venv/bin/python examples/main.py

# Remove generated build output.
clean:
    cmake --build build --target clean

# Show the available project commands.
help:
    @just --list
