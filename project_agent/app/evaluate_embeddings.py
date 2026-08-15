import argparse
from pathlib import Path

import numpy as np
import yaml
from sentence_transformers import SentenceTransformer

from app.chunker import chunk_documents
from app.loader import load_project_documents


QUESTIONS_PATH = (
    Path(__file__).resolve().parents[1]
    / "config"
    / "retrieval_questions.yaml"
)

MODELS = {
      "bge": {
          "model_id": "BAAI/bge-small-en-v1.5",
          "query_prefix": (
              "Represent this sentence for searching "
              "relevant passages: "
          ),
          "trust_remote_code": False,
      },
      "jina": {
          "model_id": "jinaai/jina-embeddings-v2-base-code",
          "query_prefix": "",
          "trust_remote_code": True,
      },
}


def load_questions() -> list[dict]:
  with QUESTIONS_PATH.open("r", encoding="utf-8") as file:
      return yaml.safe_load(file)["questions"]


def find_expected_rank(
    ranked_indices: np.ndarray,
    chunks: list,
    expected_sources: set[str],
) -> int | None:
    for rank, chunk_index in enumerate(
        ranked_indices,
        start=1,
    ):
        source = chunks[int(chunk_index)].metadata["source"]

        if source in expected_sources:
            return rank

    return None


def evaluate(model_name: str) -> None:
    config = MODELS[model_name]

    documents = load_project_documents()
    chunks = chunk_documents(documents)
    questions = load_questions()

    print(f'Model: {config["model_id"]}')
    print(f"Documents: {len(documents)}")
    print(f"Chunks: {len(chunks)}")
    print(f"Questions: {len(questions)}")

    model = SentenceTransformer(
        config["model_id"],
        trust_remote_code=config["trust_remote_code"],
    )

    if model_name == "jina":
        model.max_seq_length = 1024

    chunk_embeddings = model.encode(
        [chunk.page_content for chunk in chunks],
        normalize_embeddings=True,
        show_progress_bar=True,
    )

    query_embeddings = model.encode(
        [
            config["query_prefix"] + item["question"]
            for item in questions
        ],
        normalize_embeddings=True,
        show_progress_bar=True,
    )

    similarities = (
        np.asarray(query_embeddings)
        @ np.asarray(chunk_embeddings).T
    )

    hits = {1: 0, 3: 0, 5: 0}

    for question_index, item in enumerate(questions):
        ranked_indices = np.argsort(
            similarities[question_index]
        )[::-1][:5]

        rank = find_expected_rank(
            ranked_indices,
            chunks,
            set(item["expected_sources"]),
        )

        for cutoff in hits:
            if rank is not None and rank <= cutoff:
                hits[cutoff] += 1

        result = f"rank={rank}" if rank else "MISS"
        print(f'{item["id"]}: {result}')

        if rank is None:
            for index in ranked_indices:
                source = chunks[int(index)].metadata["source"]
                print(f"  {source}")

    count = len(questions)

    print("\nResults")

    for cutoff, hit_count in hits.items():
        percentage = 100.0 * hit_count / count

        print(
            f"Hit@{cutoff}: "
            f"{hit_count}/{count} "
            f"({percentage:.1f}%)"
        )


def main() -> None:
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--model",
        choices=MODELS,
        required=True,
    )

    arguments = parser.parse_args()
    evaluate(arguments.model)


if __name__ == "__main__":
    main()
