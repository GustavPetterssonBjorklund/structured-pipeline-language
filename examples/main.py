"""Run the example pipeline with full SPL logging and trace streaming."""

import sys

import spl


log_records = []


def on_log(record: dict) -> None:
    """Collect structured logs without taking over SPL's console formatting."""
    log_records.append(record)


def main() -> None:
    spl.setLogLevel("trace")
    spl.setLogFilter("*")
    subscription = spl.logsSubscribe(on_log)
    spl.setConsoleHandle(sys.stdout, color=True)

    pipeline = spl.load_pipeline("examples/pipeline.Calculation.splb")
    try:
        outputs = pipeline.run(
            {"x": spl.Int64(3), "y": spl.Int64(4)},
            observability="debug",
        )
        print("RESULT")
        for name, value in outputs.items():
            print(f"{name}: {value}")
    finally:
        spl.logsUnsubscribe(subscription)
        spl.clearConsoleHandle()
        print(f"Captured {len(log_records)} structured log records.")


if __name__ == "__main__":
    main()
