import argparse
import sys

from langchain_core.messages import BaseMessage

from app.augmentation import TaskMode
from app.correction import build_repair_prompt
from app.generation import (
    GenerationResult,
    ModelSelection,
    Provider,
    generate_answer,
    load_model_config,
    select_model,
)
from app.ollama_stream import stream_ollama
from app.output_validator import (
    format_validation_report,
    validate_adaptation,
)
from app.pipeline import ProjectRAGPipeline


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Drone Health Framework project assistant"
        ),
    )

    parser.add_argument(
        "--mode",
        required=True,
        choices=[
            mode.value
            for mode in TaskMode
        ],
    )

    parser.add_argument(
        "--input",
        required=True,
        help="Question or node-adaptation request",
    )

    parser.add_argument(
        "--provider",
        choices=[
            provider.value
            for provider in Provider
        ],
        help="Optional provider override",
    )

    parser.add_argument(
        "--model",
        help="Optional model-name override",
    )

    parser.add_argument(
        "--max-chunks",
        type=int,
        default=5,
    )

    parser.add_argument(
        "--repair-attempts",
        type=int,
        default=0,
        help="Retry adapt_node generation with validation errors.",
    )

    return parser.parse_args()


def generate_streamed_ollama(
    messages: list[BaseMessage],
    selection: ModelSelection,
) -> GenerationResult:
    config = load_model_config()
    tokens: list[str] = []

    print("Generating:\n")

    for token in stream_ollama(
        messages,
        selection.model,
        config,
    ):
        tokens.append(token)
        print(
            token,
            end="",
            flush=True,
        )

    print()

    return GenerationResult(
        text="".join(tokens),
        provider=selection.provider,
        model=selection.model,
    )


def generate_once(
    messages: list[BaseMessage],
    selection: ModelSelection,
) -> GenerationResult:
    if selection.provider is Provider.OLLAMA:
        return generate_streamed_ollama(
            messages,
            selection,
        )

    result = generate_answer(
        messages,
        selection,
    )
    print(result.text)
    return result


def main() -> None:
    arguments = parse_arguments()
    mode = TaskMode(arguments.mode)

    provider = (
        Provider(arguments.provider)
        if arguments.provider
        else None
    )

    pipeline = ProjectRAGPipeline()

    prepared = pipeline.prepare(
        mode=mode,
        user_input=arguments.input,
        max_chunks=arguments.max_chunks,
    )

    selection = select_model(
        mode=mode,
        provider_override=provider,
        model_override=arguments.model,
    )

    print(
        f"Provider: {selection.provider.value}\n"
        f"Model: {selection.model}\n"
    )

    try:
        result = generate_once(
            prepared.messages,
            selection,
        )
    except RuntimeError as error:
        print(
            f"\nGeneration failed: {error}",
            file=sys.stderr,
        )
        raise SystemExit(1) from error

    if mode is TaskMode.ADAPT_NODE:
        report = validate_adaptation(
            result.text
        )
        print(
            "\n"
            + format_validation_report(report)
        )

        for attempt in range(
            1,
            arguments.repair_attempts + 1,
        ):
            if report.passed:
                break

            print(f"\nRepair attempt {attempt}...\n")

            repair_prompt = build_repair_prompt(
                prepared.messages,
                result.text,
                report,
            )

            try:
                result = generate_once(
                    repair_prompt.messages,
                    selection,
                )
            except RuntimeError as error:
                print(
                    f"\nRepair generation failed: {error}",
                    file=sys.stderr,
                )
                raise SystemExit(1) from error

            report = validate_adaptation(
                result.text
            )
            print(
                "\n"
                + format_validation_report(report)
            )

    print("\nSources:")

    for source in prepared.sources:
        print(f"- {source}")


if __name__ == "__main__":
    main()
