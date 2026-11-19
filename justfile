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

# Create the local Python environment and install SPL into it.
venv:
    python3 -m venv --upgrade-deps .venv
    .venv/bin/python -m pip install -e .

# Compile the example source into a run-time artifact.
build-example: venv
    .venv/bin/python -c 'import spl; spl.build_pipeline("examples/pipeline.spl:Calculation")'

# Execute the compiled SPL example through the typed Python package.
example: build-example
    .venv/bin/python examples/main.py

# Remove generated build output.
clean:
    cmake --build build --target clean

# Show the available project commands.
help:
    @just --list
