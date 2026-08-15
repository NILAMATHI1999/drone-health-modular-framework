from pathlib import Path

import chromadb
from langchain_chroma import Chroma

from app.embeddings import BGEEmbeddings


PROJECT_AGENT_ROOT = Path(__file__).resolve().parents[1]
VECTOR_STORE_PATH = PROJECT_AGENT_ROOT / "data" / "vector_store"

CODE_COLLECTION = "drone_health_code"
DOCS_COLLECTION = "drone_health_docs"


def create_vector_stores(
    reset: bool = False,
) -> dict[str, Chroma]:
    VECTOR_STORE_PATH.mkdir(
        parents=True,
        exist_ok=True,
    )

    client = chromadb.PersistentClient(
        path=str(VECTOR_STORE_PATH),
    )

    if reset:
        existing_names = {
            collection.name
            for collection in client.list_collections()
        }

        for collection_name in (
            CODE_COLLECTION,
            DOCS_COLLECTION,
        ):
            if collection_name in existing_names:
                client.delete_collection(collection_name)

    embeddings = BGEEmbeddings()

    code_store = Chroma(
        client=client,
        collection_name=CODE_COLLECTION,
        embedding_function=embeddings,
    )

    docs_store = Chroma(
        client=client,
        collection_name=DOCS_COLLECTION,
        embedding_function=embeddings,
    )

    return {
        "code": code_store,
        "documentation": docs_store,
    }
