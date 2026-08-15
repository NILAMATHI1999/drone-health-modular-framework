
from langchain_core.embeddings import Embeddings
from sentence_transformers import SentenceTransformer


MODEL_ID = "BAAI/bge-small-en-v1.5"

QUERY_PREFIX = (
    "Represent this sentence for searching "
    "relevant passages: "
)


class BGEEmbeddings(Embeddings):
    def __init__(self) -> None:
        self.model = SentenceTransformer(MODEL_ID)

    def embed_documents(
        self,
        texts: list[str],
    ) -> list[list[float]]:
        embeddings = self.model.encode(
            texts,
            normalize_embeddings=True,
            show_progress_bar=True,
        )

        return embeddings.tolist()

    def embed_query(
        self,
        text: str,
    ) -> list[float]:
        embedding = self.model.encode(
            QUERY_PREFIX + text,
            normalize_embeddings=True,
            show_progress_bar=False,
        )

        return embedding.tolist()
