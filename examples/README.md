# SPL example

`pipeline.spl` defines a small pipeline backed by two Python nodes:

```text
sum = Add(x, y)
result = Multiply(sum, 2)
```

For `x = 3` and `y = 4`, the result is `14`.

The Python implementations are in `python_nodes/math_nodes.py`. The SPL parser
and Python worker protocol are not implemented yet, so this is an example of
the target syntax and node contract rather than a command that can currently
be executed by the SPL engine.
