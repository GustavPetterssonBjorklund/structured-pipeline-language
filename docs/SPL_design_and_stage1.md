# SPL — Structured Pipeline Language

**Design notes, architecture decisions, Stage 1 roadmap, and C++20 starter implementation**

> Status: early design proposal. SPL-IR is the canonical semantic representation. The SPL syntax and APIs shown here are illustrative and are not yet implemented or standardized.

## 1. Vision

SPL (Structured Pipeline Language) is a language for defining typed computational workflows that can be written as text, viewed and eventually edited as two-dimensional graphs, and executed across multiple programming languages. It is particularly intended for AI workflows, but should also work with conventional functions, data processing, and services.

The key idea is that the computational **graph** is the fundamental representation of a program. SPL-IR is the canonical representation of that graph. Text is one way to author it; the 2D editor is another. Neither should silently change its semantics.

A program can be modeled conceptually as:

```
Program = (Nodes, Edges, Types, RuntimeRules, VisualLayout)
```

The visual layout is separate from execution semantics: repositioning a node must not alter computation.

### Goals

- Reusable, typed nodes with named inputs and outputs.
- Graph connections and dependencies preserved in a canonical representation.
- Cross-language node execution through runtime adapters.
- A 2D view for editing, inspecting, and debugging workflows.
- Native support for AI-related operations, structured outputs, validation, and traces.
- Hierarchical composition: a whole pipeline can be used as a node.
- Clear separation between graph construction and execution.

### Non-goals for the first implementation

- Full general-purpose programming-language replacement.
- Agent-created dynamic graphs, advanced loops, distributed execution, or persistent streaming.
- Reimplementing the internals of Python, Rust, JavaScript, or other languages in SPL.

---

## 2. Core architecture

```text
             SPL source code             2D editor (later)
                    |                            |
                    v                            v
                 Parser <--------------> Canonical SPL-IR
                                               |
                                   +-----------+-----------+
                                   |                       |
                                   v                       v
                           Type validator             2D renderer
                                   |
                                   v
                              SPL scheduler
                                   |
                           Runtime adapter API
                                   |
                      +------------+------------+
                      |            |            |
                   C++ nodes   Python nodes   JS nodes
```

A clearer responsibility split:

1. **SPL-S (source):** human-readable node and pipeline definitions.
2. **SPL-IR (intermediate representation):** canonical graph containing node definitions and instances, typed ports, connections, constants, execution policy, and stable identifiers. The IR is the semantic source of truth.
3. **SPL-V (visual):** a 2D projection of the IR, including coordinates and UI metadata; layout changes do not affect execution.
4. **SPL engine:** type checks, schedules, invokes language adapters, collects outputs, and records traces.

An important architectural test is a **semantic round-trip** between source, IR, and the 2D editor. Source and visual editing must produce equivalent canonical IR. Exact formatting and comments may be preserved separately through source locations and source attachments; they are not execution semantics.

### Adopted architectural decisions

- SPL-IR is the canonical semantic source of truth.
- SPL source and the visual editor are projections of SPL-IR.
- Stage 1 supports acyclic dataflow graphs only.
- Stage 1 uses explicit primitive types: `Bool`, `Int64`, `Float64`, and `String`.
- Graph instances use explicit declarations; SPL does not use `let` or infer a node definition from an assignment.
- Implicit conversions are disabled by default; conversions must be explicit.
- Nodes are stateless and side-effect-free by default.
- Runtime failures are separate from ordinary values; domain-level failures may use `Result<T, E>`.
- Error types are instanceable typed records. Nodes declare the error types they may return, and domain errors propagate as typed `Result<T, E>` outcomes.
- Stage 1 begins with deterministic sequential execution. Parallel scheduling follows after the sequential executor is correct.
- Python is the first external runtime, accessed through persistent workers and a versioned line-delimited JSON protocol.
- Stage 1 visualization is read-only and uses both IR and execution traces.

---

## 3. SPL-owned types at node boundaries

SPL should define its own portable type system. Native implementation languages are free to use their own internal types, but values passing across node ports must conform to SPL types.

This avoids a combinatorial explosion of language-to-language converters: each adapter encodes native values to SPL and decodes SPL values into native values.

### Example type definitions (future syntax)

```spl
// Illustrative, not implemented

type String
type Bool
type Int64
type Float64
type Decimal
type Bytes

type List<T>
type Map<K, V>
type Optional<T>
type Result<T, E>

error ValidationError {
    message: String
    field: String
}

type User {
    id: String
    age: Int64
    active: Bool
}

type AIResponse {
    text: String
    confidence: Optional<Float64>
    metadata: Map<String, String>
}
```

### Distinguish three concepts

- **SPL Type:** the abstract meaning and shape, e.g. `List<User>`.
- **SPL Value:** a validated instance of an SPL type.
- **SPL Representation:** the physical format, e.g. tagged in-memory value, serialized binary, JSON-compatible payload, or shared buffer.

**Lossless conversions by default** are important. For example, a full 64-bit signed integer cannot always be represented exactly by a JavaScript `number`, so an adapter must use a safe representation (such as `bigint`) or report a conversion error. Likewise, errors, optional values, numeric precision, and ordering need explicit rules.

### Potential native mappings

| SPL | Python | TypeScript | Rust |
|---|---|---|---|
| `String` | `str` | `string` | `String` |
| `Int64` | `int` (range checked) | `bigint` | `i64` |
| `List<T>` | `list[T]` | `T[]` | `Vec<T>` |
| `Optional<T>` | `T \| None` | `T \| null` | `Option<T>` |
| `Result<T,E>` | tagged result / adapter | tagged union | `Result<T,E>` |
| `User` | dataclass / object | interface / object | struct |

The implementation should not force serialization between every node. It can use efficient native or shared-memory transfers when safe, while preserving the same logical SPL type contract.

### Future AI / runtime-related types

```spl
// Illustrative

type Artifact<T>
type Stream<T>
type Future<T>
type Secret<T>

type AIResult<T> {
    value: T
    model: String
    usage: TokenUsage
    validation: ValidationResult
}
```

Some of these are *runtime abstractions*, not ordinary serialized values. `Stream`, `Future`, and `Secret` would need special semantics and enforcement.

### Instanceable domain errors

An SPL error definition is a named, typed record. Each error occurrence is an
instance carrying validated field values:

```spl
error ValidationError {
    message: String
    field: String
}

node ValidateAge {
    input age: Int64
    output result: Result<Int64, ValidationError>

    impl python "validators.py::validate_age"
}
```

Domain errors are values and can cross runtime boundaries. Infrastructure
failures—such as worker crashes, timeouts, malformed protocol messages, or
invalid node outputs—remain engine-level execution failures. A node may only
return error types declared by its signature.

### Escape hatch

An opaque native type may be supported for compatible runtimes, but cross-language transfer should require an explicit registered converter:

```spl
type Opaque<"python", "CustomObject">
```

---

## 4. Node definitions, calls, and graph construction

### Nodes as typed functions

A node has named inputs and outputs, plus an implementation in a native language.

```spl
// Illustrative syntax
node Add {
    input a: Int64
    input b: Int64
    output sum: Int64

    impl python {
        return a + b
    }
}
```

Nodes may expose several independent output ports:

```spl
node AnalyzeText {
    input text: String
    output sentiment: String
    output confidence: Float64
    output keywords: List<String>

    impl python {
        ...
    }
}
```

### Definition versus instance versus invocation

- **Node definition:** reusable computation, e.g. `Add`.
- **Node instance:** a placement of that definition in a pipeline, e.g. `add1`.
- **Invocation:** one actual execution of an instance, important for retries, repeated runs, and future loops.

```spl
pipeline Calculation {
    input x: Int64
    input y: Int64
    output result: Int64

    use add1 = Add(a = x, b = y)
    use add2 = Add(a = add1.sum, b = 10)

    output result = add2.sum
}
```

Here `Add(...)` **declares a graph instance**; it does not immediately invoke the native function. The engine later executes the compiled pipeline.

### Three candidate execution/call styles

1. **Declarative graph construction (recommended foundation):** expressions create nodes and connections, and the scheduler executes them when ready.
2. **Imperative async calls:** `await Fetch(...)`, etc.; familiar, but can obscure opportunities for graph-level scheduling.
3. **Reactive connections:** nodes update when inputs change; useful for streaming and interactive apps, but adds lifecycle complexity.

Prefer declarative graph construction for the first version. Add explicit controlled imperative or reactive features later.

### Dependency-based scheduling

```spl
pipeline Research {
    input topic: String

    use web: SearchWeb(topic)
    use docs: SearchDatabase(topic)

    use report: Synthesize(
        web_results = web.results,
        documents = docs.results
    )

    output = report.text
}
```

`SearchWeb` and `SearchDatabase` have no dependency on each other; a later parallel scheduler can schedule them concurrently. In Stage 1, they execute in deterministic sequential order. `Synthesize` waits for both inputs.

Basic scheduler rules:

1. A node becomes ready after all required input values are available.
2. Stage 1 executes ready nodes sequentially and deterministically. Independent-node concurrency follows only after sequential execution and trace behavior are stable.
3. Node outputs are validated and published through typed ports.
4. Downstream nodes become eligible as their dependencies are satisfied.
5. Failures propagate through an explicit typed error policy or fail the pipeline.

### Composite nodes / subgraphs

A pipeline can itself implement a node:

```spl
node Research {
    input topic: String
    output report: Report

    use sources: Search(topic)
    use analysis: Analyze(sources.data)
    use summary: Summarize(analysis.result)

    report = summary.report
}
```

The visual editor can display it as one collapsed node or expand its internals.

### Loops and agents — later

Stage 1 rejects cycles. Later, prefer explicit iteration/stateful subgraph constructs to arbitrary directed cycles:

```spl
// Future feature, not Stage 1
use result: ResearchResult = repeat(max = 5) {
    use analysis: Analysis = Analyze(state)

    if analysis.done {
        break analysis.answer
    }

    state = UpdateState(previous = state, action = analysis.action)
}
```

This makes retries, cancellation, iteration limits, and inspection easier to reason about.

---

## 5. Cross-language execution

Every runtime adapter should implement a contract conceptually like:

```text
invoke(
    node_implementation,
    typed_input_values,
    execution_context
) -> typed_output_values_or_error
```

The public C++ boundary for this contract is `spl::runtime::RuntimeAdapter`.
Runtime implementations are registered through `spl::runtime::RuntimeRegistry`.
The SPL core does not embed Python, JavaScript, or other runtime-specific
execution logic.

Potential runtimes:

| Language | Suggested priority | Main use |
|---|---|---|
| C++ | Stage 1 | Native engine and first local nodes |
| Python | Stage 1 | AI, scripting, data science |
| JavaScript / TypeScript | Later | Services and UI-oriented work |
| Rust | Later | Native high-performance nodes |
| SQL | Later | Database access |
| Go, Java, C# | Later | Additional runtimes |

For an initial prototype, Python is the first external runtime and is accessed through persistent worker processes. The initial worker protocol is versioned line-delimited JSON; large or binary values use base64 initially. Later, optimized in-process adapters can use native libraries or bindings. A stable C ABI or a process protocol is preferable to exposing a compiler-specific C++ binary interface for external plugins.

The worker protocol must define request and response envelopes, protocol versioning, typed values, errors, timeouts, cancellation behavior, and worker restart behavior.

---

## 6. The 2D model

The visual layer should show not only topology but also alternate *projections* of the same graph:

- **Data view:** typed connections, schemas, actual values, and data provenance.
- **Control view:** branches, errors, retries, dependencies, and future loops.
- **Runtime view:** execution status, duration, runtime/language, and cost.

The first renderer is read-only: boxes for node instances and arrows for typed connections, with expandable port/value inspection based on the IR and execution trace. Visual editing can come in Stage 2.

A scalable editor will need hierarchical grouping, collapsing subgraphs, zoom levels, stable node IDs, and separation of display coordinates from semantic graph structure.

---

## 7. SPL language syntax options

### Option A: C/Rust-style braces — preferred

```spl
node Add {
    input a: Int;
    input b: Int;
    output result: Int;
}

use sum: Add(a = 2, b = 3);
```

Pros: familiar structure and clear boundaries. Syntax details such as semicolons still need deciding.

### Option B: indentation-based

```text
node Add:
    input a: Int
    input b: Int
    output result: Int

sum = Add(a=2, b=3)
```

Pros: compact; cons: indentation becomes semantic.

### Option C: data-oriented/YAML

```yaml
nodes:
  sum:
    type: Add
    inputs:
      a: 2
      b: 3
```

Pros: straightforward machine generation; cons: cumbersome for authoring larger programs.

### Option D: graph-oriented

```spl
sum: Add(2, 3)
double: Multiply(sum.result, 2)

sum.result -> double.a
```

Pros: graph-focused; cons: positional bindings and explicit edges can duplicate information.

**Initial choice:** braces, named-port calls, and declarative graph construction. Formatting is not part of graph semantics.

---

## 8. Engine implementation language

Candidates discussed: Rust, Go, TypeScript, Python, C++, and C#.

**Chosen direction: C++20.** C++ is a viable and strong choice for SPL's core type system, graph representation, scheduling, native execution, and FFI. Its main tradeoffs are memory safety, build/dependency complexity, concurrency discipline, and compatibility of external C++ plugin ABIs.

Suggested tools:

| Component | Initial choice |
|---|---|
| Core engine | C++20 |
| Build system | CMake |
| Parser | Handwritten lexer / recursive descent |
| Type system | C++ tagged values + SPL schemas |
| Scheduler | Initially simple dependency scheduler; thread pool later |
| Native nodes | Direct C++ functions |
| Python adapter | Worker process initially |
| JavaScript adapter | Worker process initially |
| 2D editor | TypeScript + React, or Qt if fully native |
| Testing | CTest, with Catch2 or GoogleTest as optional later additions |

The engine core should stay independent of Qt, Python, and JavaScript.

---

## 9. Stage 1: minimal working language

**Goal:** prove a typed node graph can be compiled from text, validated, executed, and displayed in 2D.

### Required features

| Capability | Stage 1 scope |
|---|---|
| Types | `Int64`, `Float64`, `Bool`, `String` |
| Composite types | `List<T>`, structs after primitives are stable |
| Nodes | Named typed inputs/outputs |
| Instances | Multiple instances per node definition |
| Connections | Output-to-input references |
| Validation | Type mismatches, missing inputs, cycle detection |
| Scheduling | Deterministic sequential topological execution; parallel execution follows later |
| Implementations | C++ initially, then Python through a persistent worker |
| Visualization | Read-only 2D graph with execution-trace and value inspection |

### Example pipeline (target syntax)

```spl
node Add {
    input a: Int
    input b: Int
    output result: Int

    impl python "math.py::add"
}

node Multiply {
    input a: Int
    input b: Int
    output result: Int

    impl python "math.py::multiply"
}

pipeline Calculation {
    input x: Int
    input y: Int
    output result: Int

    use sum: Add(a = x, b = y)
    use product: Multiply(a = sum.result, b = 2)

    result = product.result
}
```

For `x = 3` and `y = 4`, the result is `14`.

Stage 1 rejects implicit numeric conversions. If a node requires `Float64`, an `Int64` value must be converted explicitly by a future conversion node or language construct.

### Suggested build order

1. Implement SPL primitive type and value storage (detailed below).
2. Implement typed input/output port definitions.
3. Implement node definitions and instances.
4. Implement edges and graph validation.
5. Implement a deterministic sequential graph executor.
6. Add independent-node concurrency.
7. Add execution traces with invocation IDs, statuses, timestamps, values, and errors.
8. Implement the SPL text parser and compiler to IR.
9. Add the versioned Python worker protocol and verify Python ↔ SPL transfers.
10. Add a read-only 2D viewer based on IR and execution traces.

### Stage 1 acceptance criteria

- A pipeline of at least three nodes compiles and executes.
- Node instances connect through typed ports.
- Invalid types and connections are rejected with clear errors.
- The same pipeline graph can be displayed in 2D.
- Node invocation inputs and outputs can be inspected.
- A C++ node and a Python node interoperate through SPL values (for the cross-language milestone).

### Stage 1 milestones

Stage 1 is divided into independently verifiable milestones:

1. **IR core:** primitive types, values, stable IDs, ports, node definitions, instances, data edges, and serialization version.
2. **Local executor:** validation, cycle rejection, deterministic sequential execution, and runtime errors.
3. **Text front end:** parser and compiler from SPL source to canonical IR with source locations.
4. **Python runtime:** persistent worker process with a versioned line-delimited JSON protocol.
5. **Read-only viewer:** graph topology, execution status, trace data, and inspectable values.

Parallel scheduling, visual editing, control/error edges, loops, state, streams, futures, secrets, and dynamic graph mutation are later milestones.

---

# 10. First coding step: SPL Type and Value system in C++20

This is the immediate implementation milestone. It deliberately excludes parsing, graph scheduling, native-language conversion, and visual editing.

## Folder structure

```text
spl/
├── CMakeLists.txt
├── include/
│   └── spl/
│       ├── Type.hpp
│       └── Value.hpp
├── src/                 # reserved for later .cpp files
└── tests/
    └── main.cpp
```

The initial implementation is header-only; `Value.cpp` is not required yet.

## `include/spl/Type.hpp`

```cpp
#pragma once

namespace spl {

enum class Type {
    Int64,
    Float64,
    Bool,
    String
};

constexpr const char* type_name(Type type) {
    switch (type) {
        case Type::Int64:  return "Int64";
        case Type::Float64: return "Float64";
        case Type::Bool:   return "Bool";
        case Type::String: return "String";
    }
    return "Unknown";
}

} // namespace spl
```

For now `Int64` is backed by signed 64-bit storage and `Float64` by a C++ `double`. These are exact language-level names, not implicit aliases. Implicit conversions are not performed by `Value`.

## `include/spl/Value.hpp`

```cpp
#pragma once

#include "Type.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace spl {

class Value {
public:
    using Storage = std::variant<
        std::int64_t,
        double,
        bool,
        std::string
    >;

    Value(std::int64_t v) : data_(v) {}
    Value(double v) : data_(v) {}
    Value(bool v) : data_(v) {}
    Value(std::string v) : data_(std::move(v)) {}

    Type type() const {
        switch (data_.index()) {
        case 0: return Type::Int64;
        case 1: return Type::Float64;
            case 2: return Type::Bool;
            case 3: return Type::String;
            default: throw std::logic_error("Invalid SPL value");
        }
    }

    bool is(Type expected) const {
        return type() == expected;
    }

    template<typename T>
    const T& get() const {
        return std::get<T>(data_);
    }

    std::string to_string() const {
        return std::visit([](const auto& value) -> std::string {
            using T = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<T, std::string>) {
                return value;
            } else if constexpr (std::is_same_v<T, bool>) {
                return value ? "true" : "false";
            } else {
                return std::to_string(value);
            }
        }, data_);
    }

private:
    Storage data_;
};

} // namespace spl
```

This uses C++'s `std::variant` as an implementation detail, while `spl::Type` is the beginning of the language-level type system.

**Important limitation:** this first version does not yet offer safe/explicit conversion policies, recursive values, binary serialization, or a generalized runtime type registry. The `get<T>()` accessor throws if the requested native type is not held by the value. Debug formatting of floating point numbers is not yet canonical serialization.

## `tests/main.cpp`

```cpp
#include <spl/Value.hpp>

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

int main() {
    spl::Value age{std::int64_t{25}};
    spl::Value name{std::string{"Alice"}};
    spl::Value active{true};
    spl::Value score{95.5};

    assert(age.is(spl::Type::Int64));
    assert(name.is(spl::Type::String));
    assert(active.is(spl::Type::Bool));
    assert(score.is(spl::Type::Float64));

    assert(age.get<std::int64_t>() == 25);

    std::cout << spl::type_name(age.type())
              << ": " << age.to_string() << '\n';

    std::cout << spl::type_name(name.type())
              << ": " << name.to_string() << '\n';

    std::cout << spl::type_name(active.type())
              << ": " << active.to_string() << '\n';

    std::cout << spl::type_name(score.type())
              << ": " << score.to_string() << '\n';
}
```

## `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.20)

project(SPL VERSION 0.1 LANGUAGES CXX)

add_library(spl_core INTERFACE)
target_compile_features(spl_core INTERFACE cxx_std_20)

target_include_directories(spl_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(spl_test tests/main.cpp)
target_link_libraries(spl_test PRIVATE spl_core)

enable_testing()
add_test(NAME spl_core_test COMMAND spl_test)
```

## Build and test

Run from the `spl/` directory:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Run the test executable directly to see the values printed (`./build/spl_test` on common Unix-like single-config generators; executable location can vary by generator/platform).

Expected output:

```text
Int64: 25
String: Alice
Bool: true
Float64: 95.500000
```

## Success criteria for Step 1

- The project builds with a C++20 compiler.
- A value reports its SPL type correctly.
- A value is checked against an expected SPL type.
- Typed retrieval works for matching native storage.
- The four primitive values can be printed for debugging.
- The CTest test passes.

## Next step

Define `Port` with a stable name, direction (`Input` / `Output`), and SPL type. Then define a node signature from its ports, and only after that implement connections and graph validation.

---

## 11. Remaining design questions

The following decisions are intentionally deferred until after the Stage 1 core:

1. How will `Null`, optional values, structured records, and recursive values be represented?
2. What is the complete runtime error and domain-error propagation model?
3. How are side effects, caching, determinism, and idempotence declared in the IR?
4. What explicit construct represents stateful nodes?
5. When may an adapter use zero-copy in-process memory?
6. How are control edges, error edges, retries, and cancellation represented?
7. Which graph changes, if any, are legal at runtime?
8. How are source attachments and formatting preserved across source/IR projections?
9. What are the semantics of streams, futures, secrets, and loops?

**Current path:** C++20 core → SPL values → versioned IR → ports → nodes → graph validator → deterministic executor and traces → parser → Python worker → read-only 2D viewer → parallel scheduling → additional runtimes → advanced AI and reactive features.
