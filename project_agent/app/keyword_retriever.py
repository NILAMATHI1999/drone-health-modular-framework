import re

import numpy as np
from rank_bm25 import BM25Okapi

from app.chunker import chunk_documents
from app.loader import load_project_documents
from app.retriever import RetrievalResult


TOKEN_PATTERN = re.compile(
    r"/[A-Za-z0-9_./-]+"
    r"|[A-Za-z_][A-Za-z0-9_.:/-]*"
    r"|\d+"
)

CAMEL_CASE_PATTERN = re.compile(
    r"(?<=[a-z0-9])(?=[A-Z])"
)


def tokenize(text: str) -> list[str]:
    tokens: list[str] = []

    for match in TOKEN_PATTERN.findall(text):
        tokens.append(match.lower())

        camel_parts = CAMEL_CASE_PATTERN.split(match)

        if len(camel_parts) > 1:
            tokens.extend(
                part.lower()
                for part in camel_parts
                if part
            )

        identifier_parts = re.split(
            r"[_./:-]+",
            match,
        )

        tokens.extend(
            part.lower()
            for part in identifier_parts
            if part
        )

    return tokens


class KeywordRetriever:
    def __init__(self) -> None:
        documents = load_project_documents()
        self.chunks = chunk_documents(documents)

        tokenized_chunks = [
            tokenize(chunk.page_content)
            for chunk in self.chunks
        ]

        self.index = BM25Okapi(tokenized_chunks)

    def retrieve(
        self,
        query: str,
        limit: int = 5,
    ) -> list[RetrievalResult]:
        query_tokens = tokenize(query)
        scores = self.index.get_scores(query_tokens)

        ranked_indices = np.argsort(scores)[::-1]
        results: list[RetrievalResult] = []

        for chunk_index in ranked_indices:
            score = float(scores[chunk_index])

            if score <= 0:
                continue

            results.append(
                RetrievalResult(
                    document=self.chunks[int(chunk_index)],
                    score=score,
                    method="keyword",
                )
            )

            if len(results) == limit:
                break

        return results
