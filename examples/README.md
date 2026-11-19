# SPL example

`pipeline.spl` defines a small pipeline backed by Python, JavaScript, and
TypeScript nodes:

```text
sum = Add(x, y)
product = Multiply(sum, 2)
result = Increment(product)
```

For `x = 3` and `y = 4`, the result is `15`.

Run it through the typed Python package with:

```bash
just example
```

`just example` first runs `build/spl build` to create `pipeline.Calculation.splb`
and then runs that compiled artifact. Runtime execution does not parse
`pipeline.spl`.

The inputs use explicit SPL types (`spl.Int64`) and the returned outputs are
typed SPL values as well.

The Python implementations are in `python_nodes/math_nodes.py`. They import
`spl`, receive typed SPL values as named parameters, and return typed SPL
values directly. The embedded Python adapter invokes them in-process.

`javascript_nodes/math_nodes.js` contains the JavaScript multiplication step.
`typescript_nodes/math_nodes.ts` contains the TypeScript increment step;
`tsc` compiles it automatically when the runtime invokes it.

`examples/main.py` also demonstrates SPL-owned console rendering. It installs
`sys.stdout` with `spl.setConsoleHandle(..., color=True)`, enables DEBUG
observability, and subscribes separately to structured log records for
collection. SPL remains responsible for formatting and coloring the console
output.
