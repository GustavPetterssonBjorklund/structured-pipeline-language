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

# Build and execute the SPL example through the C++ engine.
example:
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
    cmake --build build --target spl-run
    ./build/spl-run examples/pipeline.spl

# Remove generated build output.
clean:
    cmake --build build --target clean

# Show the available project commands.
help:
    @just --list
