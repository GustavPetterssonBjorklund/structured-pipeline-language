"""Typed Python implementations for examples/pipeline.spl."""

import spl


def add(a: spl.Int64, b: spl.Int64) -> spl.Int64:
    """Return the sum of two Int64 SPL inputs."""
    return a + b


def multiply(a: spl.Int64, b: spl.Int64) -> spl.Int64:
    """Return the product of two Int64 SPL inputs."""
    return a * b
