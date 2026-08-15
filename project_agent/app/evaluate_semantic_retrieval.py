from pathlib import Path

import yaml

from app.retriever import SemanticRetriever


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


def evaluate() -> None:
    questions = load_questions()
    retriever = SemanticRetriever()
    hits = {1: 0, 3: 0, 5: 0}

    for item in questions:
        results = retriever.retrieve(
            item["question"],
            limit=5,
        )

        expected_sources = set(
            item["expected_sources"]
        )

        rank = None

        for index, result in enumerate(
            results,
            start=1,
        ):
            source = result.document.metadata["source"]

            if source in expected_sources:
                rank = index
                break

        for cutoff in hits:
            if rank is not None and rank <= cutoff:
                hits[cutoff] += 1

        outcome = (
            f"rank={rank}"
            if rank is not None
            else "MISS"
        )

        print(f'{item["id"]}: {outcome}')

        if rank is None:
            for result in results:
                source = result.document.metadata["source"]

                print(
                    f"  {source} "
                    f"score={result.score:.4f}"
                )

    question_count = len(questions)

    print("\nSemantic retrieval results")

    for cutoff, count in hits.items():
        percentage = (
            100.0 * count / question_count
        )

        print(
            f"Hit@{cutoff}: "
            f"{count}/{question_count} "
            f"({percentage:.1f}%)"
        )


if __name__ == "__main__":
    evaluate()
