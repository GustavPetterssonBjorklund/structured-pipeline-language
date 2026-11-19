"""Python implementations for examples/pipeline.spl."""


def add(inputs: dict[str, int]) -> dict[str, int]:
    """Return the sum of two Int64 SPL inputs."""
    return {"sum": inputs["a"] + inputs["b"]}


def multiply(inputs: dict[str, int]) -> dict[str, int]:
    """Return the product of two Int64 SPL inputs."""
    return {"product": inputs["a"] * inputs["b"]}
