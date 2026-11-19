# SPL example

`pipeline.spl` defines a small pipeline backed by two Python nodes:

```text
sum = Add(x, y)
result = Multiply(sum, 2)
```

For `x = 3` and `y = 4`, the result is `14`.

The Python implementations are in `python_nodes/math_nodes.py`. The C++ SPL
engine parses the pipeline, resolves the declared node implementations, and
invokes them through `runtimes/python/worker.py`.

Run the current Python-backed example from the project root with:

```bash
just example
```
