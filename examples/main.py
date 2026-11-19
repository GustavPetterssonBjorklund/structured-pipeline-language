"""Run the example pipeline through the typed Python SPL package."""

import spl


def main() -> None:
    pipeline = spl.load_pipeline("examples/pipeline.spl:Calculation")
    outputs = pipeline.run({
        "x": spl.Int64(3),
        "y": spl.Int64(4),
    })

    for name, value in outputs.items():
        print(f"{name}: {value}")


if __name__ == "__main__":
    main()
