# Python runtime

This directory contains the Python-side runtime boundary for SPL.

The planned worker protocol is versioned line-delimited JSON over stdin/stdout.
Python node implementations remain in the example or user project; this
directory contains runtime machinery, not application nodes.
