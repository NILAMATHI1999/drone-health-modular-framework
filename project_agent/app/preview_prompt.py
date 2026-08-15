
import argparse
from pathlib import Path

from app.augmentation import TaskMode
from app.pipeline import ProjectRAGPipeline


def read_user_input(
    direct_input: str | None,
    input_file: str | None,
) -> str:
    if input_file:
        return Path(input_file).read_text(
            encoding="utf-8"
        )

    if direct_input:
        return direct_input

    raise ValueError("User input is required")


def main() -> None:
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--mode",
        choices=[
            mode.value
            for mode in TaskMode
        ],
        required=True,
    )

    input_group = parser.add_mutually_exclusive_group(
        required=True
    )

    input_group.add_argument("--input")
    input_group.add_argument("--input-file")

    parser.add_argument(
        "--max-chunks",
        type=int,
        default=5,
        choices=range(1, 6),
    )

    arguments = parser.parse_args()

    user_input = read_user_input(
        arguments.input,
        arguments.input_file,
    )

    pipeline = ProjectRAGPipeline()

    prepared = pipeline.prepare(
        TaskMode(arguments.mode),
        user_input,
        max_chunks=arguments.max_chunks,
    )

    for message in prepared.messages:
        print(f"\n--- {message.type.upper()} ---")
        print(message.content)

    print("\n--- RETRIEVED SOURCES ---")

    for source in prepared.sources:
        print(source)


if __name__ == "__main__":
    main()
