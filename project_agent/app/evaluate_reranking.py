from pathlib import Path

import yaml

from app.reranking_retriever import (
    METHODS,
    RerankingRetriever,
)


QUESTIONS_PATH = (
    Path(__file__).resolve().parents[1]
    / "config"
    / "retrieval_questions.yaml"
)


def load_questions() -> list[dict]:
    with QUESTIONS_PATH.open(
        "r",
        encoding="utf-8",
    ) as file:
        return yaml.safe_load(file)["questions"]


def expected_rank(
    results: list,
    expected_sources: set[str],
) -> int | None:
    for rank, result in enumerate(
        results,
        start=1,
    ):
        source = result.document.metadata["source"]

        if source in expected_sources:
            return rank

    return None


def evaluate() -> None:
    questions = load_questions()
    retriever = RerankingRetriever()

    hits = {
        method: {1: 0, 3: 0, 5: 0}
        for method in METHODS
    }

    for item in questions:
        method_results = (
            retriever.retrieve_all_methods(
                item["question"],
                limit=5,
                candidate_limit=20,
            )
        )

        expected_sources = set(
            item["expected_sources"]
        )

        outcomes = []

        for method in METHODS:
            rank = expected_rank(
                method_results[method],
                expected_sources,
            )

            for cutoff in hits[method]:
                if (
                    rank is not None
                    and rank <= cutoff
                ):
                    hits[method][cutoff] += 1

            outcome = (
                str(rank)
                if rank is not None
                else "MISS"
            )

            outcomes.append(
                f"{method}={outcome}"
            )

        print(
            f'{item["id"]}: '
            + ", ".join(outcomes)
        )

    question_count = len(questions)

    print("\nReranking comparison")

    for method in METHODS:
        print(f"\n{method}")

        for cutoff, count in hits[method].items():
            percentage = (
                100.0
                * count
                / question_count
            )

            print(
                f"Hit@{cutoff}: "
                f"{count}/{question_count} "
                f"({percentage:.1f}%)"
            )


if __name__ == "__main__":
    evaluate()
