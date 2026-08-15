from dataclasses import dataclass

from langchain_core.messages import BaseMessage

from app.adaptation_prompt import (
    build_adaptation_prompt,
)
from app.augmentation import (
    TaskMode,
    build_messages,
)
from app.retriever import RetrievalResult
from app.task_retriever import TaskAwareRetriever


@dataclass(frozen=True)
class PreparedPrompt:
    messages: list[BaseMessage]
    sources: list[str]
    results: list[RetrievalResult]


class ProjectRAGPipeline:
    def __init__(
        self,
        retriever: TaskAwareRetriever | None = None,
    ) -> None:
        self._retriever = retriever

    @property
    def retriever(self) -> TaskAwareRetriever:
        if self._retriever is None:
            self._retriever = TaskAwareRetriever()

        return self._retriever

    def prepare(
        self,
        mode: TaskMode,
        user_input: str,
        max_chunks: int = 5,
    ) -> PreparedPrompt:
        if mode is TaskMode.ADAPT_NODE:
            adaptation = build_adaptation_prompt(
                user_input
            )

            return PreparedPrompt(
                messages=adaptation.messages,
                sources=adaptation.sources,
                results=[],
            )

        results = self.retriever.retrieve(
            mode=mode,
            user_input=user_input,
            limit=max_chunks,
        )

        messages, sources = build_messages(
            mode,
            user_input,
            results,
            max_chunks=max_chunks,
        )

        return PreparedPrompt(
            messages=messages,
            sources=sources,
            results=results,
        )
