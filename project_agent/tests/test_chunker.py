from langchain_core.documents import Document

from app.chunker import CHUNK_SIZE, chunk_document


def make_document(text: str, file_type: str = "cpp") -> Document:
    return Document(
        page_content=text,
        metadata={
            "source": "src/example.cpp",
            "package": "example",
            "file_name": "example.cpp",
            "file_type": file_type,
            "content_type": "code",
            "template_type": "none",
        },
    )


def test_small_document_remains_whole() -> None:
    chunks = chunk_document(make_document("line one\nline two\n"))

    assert len(chunks) == 1
    assert chunks[0].metadata["start_line"] == 1
    assert chunks[0].metadata["end_line"] == 3


def test_large_document_is_split_with_metadata() -> None:
    text = "\n\n".join(
        f"void function_{index}() {{ return; }}" for index in range(100)
    )
    chunks = chunk_document(make_document(text))

    assert len(chunks) > 1
    assert all(len(chunk.page_content) <= CHUNK_SIZE for chunk in chunks)
    assert [chunk.metadata["chunk_index"] for chunk in chunks] == list(
        range(len(chunks))
    )
    assert all(chunk.metadata["start_line"] >= 1 for chunk in chunks)
    assert all(
        chunk.metadata["end_line"] >= chunk.metadata["start_line"]
        for chunk in chunks
    )


def test_interface_file_is_preserved() -> None:
    text = "string value\n" * 200
    chunks = chunk_document(make_document(text, file_type="srv"))

    assert len(chunks) == 1
