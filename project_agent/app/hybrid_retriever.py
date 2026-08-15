from collections.abc import Sequence

from app.keyword_retriever import KeywordRetriever
from app.retriever import (
    RetrievalResult,
    SemanticRetriever,
)


RRF_CONSTANT = 60

METHODS = (
    "equal_rrf",
    "weighted_rrf",
    "relative_score",
)


def chunk_identity(
    result: RetrievalResult,
) -> tuple[str, int]:
    metadata = result.document.metadata

    return (
        metadata["source"],
        metadata["chunk_index"],
    )


def normalize_scores(
    results: Sequence[RetrievalResult],
    lower_is_better: bool,
) -> dict[tuple[str, int], float]:
    if not results:
        return {}

    values = [
        result.score
        for result in results
    ]

    minimum = min(values)
    maximum = max(values)

    if maximum == minimum:
        return {
            chunk_identity(result): 1.0
            for result in results
        }

    normalized = {}

    for result in results:
        if lower_is_better:
            score = (
                maximum - result.score
            ) / (
                maximum - minimum
            )
        else:
            score = (
                result.score - minimum
            ) / (
                maximum - minimum
            )

        normalized[chunk_identity(result)] = score

    return normalized


class HybridRetriever:
    def __init__(self) -> None:
        self.semantic = SemanticRetriever()
        self.keyword = KeywordRetriever()

    def retrieve_all_methods(
        self,
        query: str,
        limit: int = 5,
        pool_size: int = 10,
    ) -> dict[str, list[RetrievalResult]]:
        semantic_results = self.semantic.retrieve(
            query,
            limit=pool_size,
        )

        keyword_results = self.keyword.retrieve(
            query,
            limit=pool_size,
        )

        return {
            "equal_rrf": self._rank_fusion(
                semantic_results,
                keyword_results,
                semantic_weight=0.5,
                keyword_weight=0.5,
                limit=limit,
                method="equal_rrf",
            ),
            "weighted_rrf": self._rank_fusion(
                semantic_results,
                keyword_results,
                semantic_weight=0.7,
                keyword_weight=0.3,
                limit=limit,
                method="weighted_rrf",
            ),
            "relative_score": (
                self._relative_score_fusion(
                    semantic_results,
                    keyword_results,
                    semantic_weight=0.7,
                    keyword_weight=0.3,
                    limit=limit,
                )
            ),
        }

    def _collect_documents(
        self,
        semantic_results: Sequence[RetrievalResult],
        keyword_results: Sequence[RetrievalResult],
    ) -> dict[tuple[str, int], RetrievalResult]:
        documents = {}

        for result in (
            list(semantic_results)
            + list(keyword_results)
        ):
            documents.setdefault(
                chunk_identity(result),
                result,
            )

        return documents

    def _rank_fusion(
        self,
        semantic_results: Sequence[RetrievalResult],
        keyword_results: Sequence[RetrievalResult],
        semantic_weight: float,
        keyword_weight: float,
        limit: int,
        method: str,
    ) -> list[RetrievalResult]:
        documents = self._collect_documents(
            semantic_results,
            keyword_results,
        )

        fused_scores = {
            identity: 0.0
            for identity in documents
        }

        for rank, result in enumerate(
            semantic_results,
            start=1,
        ):
            identity = chunk_identity(result)

            fused_scores[identity] += (
                semantic_weight
                / (RRF_CONSTANT + rank)
            )

        for rank, result in enumerate(
            keyword_results,
            start=1,
        ):
            identity = chunk_identity(result)

            fused_scores[identity] += (
                keyword_weight
                / (RRF_CONSTANT + rank)
            )

        return self._build_results(
            documents,
            fused_scores,
            method,
            limit,
        )

    def _relative_score_fusion(
        self,
        semantic_results: Sequence[RetrievalResult],
        keyword_results: Sequence[RetrievalResult],
        semantic_weight: float,
        keyword_weight: float,
        limit: int,
    ) -> list[RetrievalResult]:
        documents = self._collect_documents(
            semantic_results,
            keyword_results,
        )

        semantic_scores = normalize_scores(
            semantic_results,
            lower_is_better=True,
        )

        keyword_scores = normalize_scores(
            keyword_results,
            lower_is_better=False,
        )

        fused_scores = {}

        for identity in documents:
            fused_scores[identity] = (
                semantic_weight
                * semantic_scores.get(identity, 0.0)
                + keyword_weight
                * keyword_scores.get(identity, 0.0)
            )

        return self._build_results(
            documents,
            fused_scores,
            "relative_score",
            limit,
        )

    def _build_results(
        self,
        documents: dict[
            tuple[str, int],
            RetrievalResult,
        ],
        scores: dict[tuple[str, int], float],
        method: str,
        limit: int,
    ) -> list[RetrievalResult]:
        ranked_identities = sorted(
            scores,
            key=scores.get,
            reverse=True,
        )

        return [
            RetrievalResult(
                document=documents[identity].document,
                score=scores[identity],
                method=method,
            )
            for identity in ranked_identities[:limit]
        ]
