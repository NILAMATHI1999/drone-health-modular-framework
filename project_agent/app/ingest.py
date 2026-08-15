import hashlib

from langchain_core.documents import Document

from app.chunker import chunk_documents
from app.loader import load_project_documents
from app.vector_store import create_vector_stores


def make_chunk_id(document: Document) -> str:
    identity = (
        f'{document.metadata["source"]}:'
        f'{document.metadata["chunk_index"]}:'
        f"{document.page_content}"
    )

    return hashlib.sha256(
        identity.encode("utf-8")
    ).hexdigest()


def ingest_project() -> None:
    documents = load_project_documents()
    chunks = chunk_documents(documents)

    code_chunks = [
        chunk
        for chunk in chunks
        if chunk.metadata["content_type"]
        != "documentation"
    ]

    documentation_chunks = [
        chunk
        for chunk in chunks
        if chunk.metadata["content_type"]
        == "documentation"
    ]

    stores = create_vector_stores(reset=True)

    if code_chunks:
        stores["code"].add_documents(
            documents=code_chunks,
            ids=[
                make_chunk_id(chunk)
                for chunk in code_chunks
            ],
        )

    if documentation_chunks:
        stores["documentation"].add_documents(
            documents=documentation_chunks,
            ids=[
                make_chunk_id(chunk)
                for chunk in documentation_chunks
            ],
        )

    print(f"Loaded files: {len(documents)}")
    print(f"Total chunks: {len(chunks)}")
    print(f"Code chunks: {len(code_chunks)}")
    print(
        "Documentation chunks: "
        f"{len(documentation_chunks)}"
    )
    print("ChromaDB ingestion completed")


if __name__ == "__main__":
    ingest_project()
