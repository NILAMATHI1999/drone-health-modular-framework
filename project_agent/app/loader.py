from pathlib import Path
from typing import Any

import yaml
from langchain_core.documents import Document


PROJECT_ROOT = Path(__file__).resolve().parents[2]
CONFIG_PATH = (
    Path(__file__).resolve().parents[1]
    / "config"
    / "sources.yaml"
)


def load_source_config() -> dict[str, Any]:
    with CONFIG_PATH.open("r", encoding="utf-8") as file:
        return yaml.safe_load(file)


def is_allowed_file(path: Path, config: dict[str, Any]) -> bool:
    relative_path = path.relative_to(PROJECT_ROOT)

    excluded = set(config["exclude_directories"])
    if any(part in excluded for part in relative_path.parts):
        return False

    allowed_names = set(config["include_names"])
    allowed_extensions = set(config["include_extensions"])

    return (
        path.name in allowed_names
        or path.suffix.lower() in allowed_extensions
    )


def detect_package(relative_path: Path) -> str:
    parts = relative_path.parts

    if len(parts) >= 2 and parts[0] == "src":
        return parts[1]

    return "repository"


def detect_content_type(path: Path) -> str:
    if path.suffix == ".md":
        return "documentation"

    if path.suffix in {".yaml", ".yml", ".xml"}:
        return "configuration"

    if path.suffix in {".msg", ".srv"}:
        return "interface"

    if path.name in {"CMakeLists.txt", "package.xml"}:
        return "build"

    return "code"


def detect_template_type(relative_path: Path) -> str:
    name = relative_path.name.lower()

    if "publisher_template" in name:
        return "publisher"

    if "subscriber_template" in name:
        return "subscriber"

    if "service_template" in name:
        return "service"

    return "none"


def load_project_documents() -> list[Document]:
    config = load_source_config()
    documents: list[Document] = []

    for path in sorted(PROJECT_ROOT.rglob("*")):
        if not path.is_file() or not is_allowed_file(path, config):
            continue

        try:
            content = path.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue

        if not content.strip():
            continue

        relative_path = path.relative_to(PROJECT_ROOT)

        documents.append(
            Document(
                page_content=content,
                metadata={
                    "source": str(relative_path),
                    "package": detect_package(relative_path),
                    "file_name": path.name,
                    "file_type": path.suffix.lstrip(".") or path.name,
                    "content_type": detect_content_type(path),
                    "template_type": detect_template_type(relative_path),
                },
            )
        )

    return documents


if __name__ == "__main__":
    loaded_documents = load_project_documents()

    print(f"Loaded {len(loaded_documents)} project files")

    for document in loaded_documents[:10]:
        print(document.metadata["source"])
