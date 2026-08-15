from langchain_core.documents import Document
from langchain_text_splitters import RecursiveCharacterTextSplitter


CHUNK_SIZE = 1500
CHUNK_OVERLAP = 200


def separators_for(document: Document) -> list[str]:
    file_type = document.metadata["file_type"]

    if file_type == "md":
        return ["\n## ", "\n### ", "\n\n", "\n", " ", ""]

    if file_type in {"cpp", "hpp", "h"}:
        return [
            "\nclass ",
            "\nstruct ",
            "\n  void ",
            "\nvoid ",
            "\n\n",
            "\n",
            " ",
            "",
        ]

    if file_type == "py":
        return ["\nclass ", "\ndef ", "\n\n", "\n", " ", ""]

    if file_type in {"yaml", "yml", "xml", "CMakeLists.txt", "package.xml"}:
        return ["\n\n", "\n", " ", ""]

    return ["\n\n", "\n", " ", ""]


def line_number(text: str, character_index: int) -> int:
    return text.count("\n", 0, character_index) + 1


def chunk_document(document: Document) -> list[Document]:
    text = document.page_content
    file_type = document.metadata["file_type"]

    if len(text) <= CHUNK_SIZE or file_type in {"msg", "srv"}:
        metadata = dict(document.metadata)
        metadata.update(
            {
                "chunk_index": 0,
                "start_line": 1,
                "end_line": text.count("\n") + 1,
            }
        )
        return [Document(page_content=text, metadata=metadata)]

    splitter = RecursiveCharacterTextSplitter(
        chunk_size=CHUNK_SIZE,
        chunk_overlap=CHUNK_OVERLAP,
        separators=separators_for(document),
        add_start_index=True,
    )
    chunks = splitter.split_documents([document])

    for index, chunk in enumerate(chunks):
        start_index = int(chunk.metadata.pop("start_index"))
        end_index = start_index + len(chunk.page_content)
        chunk.metadata.update(
            {
                "chunk_index": index,
                "start_line": line_number(text, start_index),
                "end_line": line_number(text, end_index),
            }
        )

    return chunks


def chunk_documents(documents: list[Document]) -> list[Document]:
    chunks: list[Document] = []

    for document in documents:
        chunks.extend(chunk_document(document))

    return chunks
