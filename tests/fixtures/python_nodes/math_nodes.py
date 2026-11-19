"""Typed Python implementations for the mixed-runtime test fixture."""

import spl


def add(a: spl.Int64, b: spl.Int64) -> spl.Int64:
    return a + b
