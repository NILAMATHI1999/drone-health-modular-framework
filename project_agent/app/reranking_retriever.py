from collections.abc import Sequence

from sentence_transformers import CrossEncoder

from app.hybrid_retriever import HybridRetriever
from app.retriever import (
    RetrievalResult,
    SemanticRetriever,
)


RERANKER_MODEL_ID = (
    "cross-encoder/ms-marco-MiniLM-L6-v2"
)

METHODS = (
    "semantic_rerank",
    "hybrid_rerank",
)


class RerankingRetriever:
    def __init__(self) -> None:
        self.semantic = SemanticRetriever()
        self.hybrid = HybridRetriever()

        self.reranker = CrossEncoder(
            RERANKER_MODEL_ID
        )

    def _rerank(
        self,
        query: str,
        candidates: Sequence[RetrievalResult],
        method: str,
        limit: int,
    ) -> list[RetrievalResult]:
        if not candidates:
            return []

        pairs = [
            (
                query,
                result.document.page_content,
            )
            for result in candidates
        ]

        scores = self.reranker.predict(
            pairs,
            show_progress_bar=False,
        )

        reranked = [
            RetrievalResult(
                document=result.document,
                score=float(score),
                method=method,
            )
            for result, score in zip(
                candidates,
                scores,
                strict=True,
            )
        ]

        reranked.sort(
            key=lambda result: result.score,
            reverse=True,
        )

        return reranked[:limit]

    def retrieve_all_methods(
        self,
        query: str,
        limit: int = 5,
        candidate_limit: int = 20,
    ) -> dict[str, list[RetrievalResult]]:
        semantic_candidates = (
            self.semantic.retrieve(
                query,
                limit=candidate_limit,
            )
        )

        hybrid_candidates = (
            self.hybrid.retrieve_all_methods(
                query,
                limit=candidate_limit,
                pool_size=candidate_limit // 2,
            )["relative_score"]
        )

        return {
            "semantic_rerank": self._rerank(
                query,
                semantic_candidates,
                "semantic_rerank",
                limit,
            ),
            "hybrid_rerank": self._rerank(
                query,
                hybrid_candidates,
                "hybrid_rerank",
                limit,
            ),
        }
