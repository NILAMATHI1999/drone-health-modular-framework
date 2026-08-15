from dataclasses import dataclass

from langchain_core.documents import Document

from app.vector_store import create_vector_stores


@dataclass(frozen=True)
class RetrievalResult:
    document: Document
    score: float
    method: str


class SemanticRetriever:
    def __init__(self) -> None:
        self.stores = create_vector_stores(reset=False)

    def retrieve(
        self,
        query: str,
        limit: int = 5,
    ) -> list[RetrievalResult]:
        candidates: list[RetrievalResult] = []

        for store in self.stores.values():
            matches = store.similarity_search_with_score(
                query,
                k=limit,
            )

            candidates.extend(
                RetrievalResult(
                    document=document,
                    score=float(score),
                    method="semantic",
                )
                for document, score in matches
            )

        candidates.sort(
            key=lambda result: result.score
        )

        unique_results: list[RetrievalResult] = []
        seen_chunks: set[tuple[str, int]] = set()

        for result in candidates:
            identity = (
                result.document.metadata["source"],
                result.document.metadata["chunk_index"],
            )

            if identity in seen_chunks:
                continue

            seen_chunks.add(identity)
            unique_results.append(result)

            if len(unique_results) == limit:
                break

        return unique_results
