# Python worker runtime (legacy isolation backend)

This directory contains the optional process-isolated Python worker. The
default Python adapter now embeds CPython and calls typed Python nodes
directly; this worker remains available for a future explicitly isolated
adapter.

The worker currently accepts a versioned JSON request through `--input-json`
and emits a versioned JSON response. Values are tagged with their SPL type so
`Bool`, `Int64`, `Float64`, and `String` can cross the runtime boundary without
being coerced to integers.

Python node implementations remain in the example or user project; this
directory contains runtime machinery, not application nodes. The C++ adapter
still launches one worker process per invocation; persistent worker lifecycle,
timeouts, and cancellation remain follow-up runtime work.
