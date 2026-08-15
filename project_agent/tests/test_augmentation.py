from langchain_core.documents import Document

from app.augmentation import (
    TaskMode,
    build_context,
    build_messages,
)
from app.retriever import RetrievalResult


def make_result(
    source: str,
    chunk_index: int,
) -> RetrievalResult:
    document = Document(
        page_content="example project content",
        metadata={
            "source": source,
            "package": "example_package",
            "file_name": "example.cpp",
            "file_type": "cpp",
            "content_type": "code",
            "template_type": "publisher",
            "chunk_index": chunk_index,
            "start_line": 10,
            "end_line": 20,
        },
    )

    return RetrievalResult(
        document=document,
        score=0.5,
        method="relative_score",
    )


def test_context_contains_source_metadata() -> None:
    context, sources = build_context(
        [make_result("src/example.cpp", 0)]
    )

    assert "Path: src/example.cpp" in context
    assert "Lines: 10-20" in context
    assert "example project content" in context
    assert sources == ["src/example.cpp"]


def test_context_respects_chunk_limit() -> None:
    results = [
        make_result(f"src/example_{index}.cpp", index)
        for index in range(10)
    ]

    context, sources = build_context(
        results,
        max_chunks=3,
    )

    assert len(sources) == 3
    assert "SOURCE 3" in context
    assert "SOURCE 4" not in context


def test_messages_include_adaptation_contract() -> None:
    messages, sources = build_messages(
        TaskMode.ADAPT_NODE,
        "Adapt this publisher.",
        [make_result("src/template.cpp", 0)],
    )

    assert len(messages) == 2
    assert "adapt_node" in messages[1].content
    assert "matching YAML" in messages[1].content
    assert sources == ["src/template.cpp"]


def test_empty_user_input_is_rejected() -> None:
    try:
        build_messages(
            TaskMode.EXPLAIN_PROJECT,
            "   ",
            [],
        )
    except ValueError as error:
        assert str(error) == "user_input must not be empty"
    else:
        raise AssertionError("Expected ValueError")
