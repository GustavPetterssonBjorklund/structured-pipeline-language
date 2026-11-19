# SPL example

`pipeline.spl` defines a small pipeline backed by two Python nodes:

```text
sum = Add(x, y)
result = Multiply(sum, 2)
```

For `x = 3` and `y = 4`, the result is `14`.

Run it through the typed Python package with:

```bash
just example
```

The inputs use explicit SPL types (`spl.Int64`) and the returned outputs are
typed SPL values as well.

The Python implementations are in `python_nodes/math_nodes.py`. They import
`spl`, receive typed SPL values as named parameters, and return typed SPL
values directly. The embedded Python adapter invokes them in-process.

Run the current Python-backed example from the project root with:

```bash
just example
```
