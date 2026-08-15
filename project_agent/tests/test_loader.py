from app.loader import load_project_documents


def test_loader_finds_curated_project_files() -> None:
    documents = load_project_documents()
    sources = {document.metadata["source"] for document in documents}

    assert "README.md" in sources
    assert "src/drone_health_core/CMakeLists.txt" in sources
    assert any(source.endswith("MonitorSpec.msg") for source in sources)


def test_loader_excludes_generated_and_agent_files() -> None:
    documents = load_project_documents()

    for document in documents:
        parts = document.metadata["source"].split("/")
        assert not {"build", "install", "log", "tools", "project_agent"} & set(parts)
