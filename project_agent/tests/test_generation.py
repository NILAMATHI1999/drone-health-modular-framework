import pytest
from langchain_core.messages import (
    HumanMessage,
    SystemMessage,
)

from app.augmentation import TaskMode
from app.generation import (
    Provider,
    generate_with_gemini,
    generate_with_groq,
    select_model,
)


CONFIG = {
    "routing": {
        "explain_project": {
            "provider": "ollama",
            "model": "llama3.2:3b",
        },
        "adapt_node": {
            "provider": "gemini",
            "model": "gemini-3.5-flash-lite",
        },
        "reuse_component": {
            "provider": "gemini",
            "model": "gemini-3.5-flash-lite",
        },
    },
    "providers": {
        "ollama": {
            "base_url": "http://localhost:11434",
        },
        "gemini": {
            "model": "gemini-test",
        },
        "groq": {
            "base_url": "https://api.groq.com/openai/v1",
            "model": "qwen-test",
        },
    },
    "generation": {
        "temperature": 0.1,
        "context_window": 8192,
        "max_output_tokens": 4096,
    },
}


def test_explanation_routes_to_llama() -> None:
    selection = select_model(
        TaskMode.EXPLAIN_PROJECT,
        config=CONFIG,
    )

    assert selection.provider is Provider.OLLAMA
    assert selection.model == "llama3.2:3b"


def test_adaptation_routes_to_gemini_flash_lite() -> None:
    selection = select_model(
        TaskMode.ADAPT_NODE,
        config=CONFIG,
    )

    assert selection.provider is Provider.GEMINI
    assert selection.model == "gemini-3.5-flash-lite"


def test_gemini_override() -> None:
    selection = select_model(
        TaskMode.ADAPT_NODE,
        provider_override=Provider.GEMINI,
        config=CONFIG,
    )

    assert selection.provider is Provider.GEMINI
    assert selection.model == "gemini-test"


def test_gemini_requires_api_key(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.delenv(
        "GEMINI_API_KEY",
        raising=False,
    )

    messages = [
        SystemMessage(content="Rules"),
        HumanMessage(content="Question"),
    ]

    with pytest.raises(
        RuntimeError,
        match="GEMINI_API_KEY",
    ):
        generate_with_gemini(
            messages,
            "gemini-test",
            CONFIG,
        )


def test_groq_override() -> None:
    selection = select_model(
        TaskMode.ADAPT_NODE,
        provider_override=Provider.GROQ,
        config=CONFIG,
    )

    assert selection.provider is Provider.GROQ
    assert selection.model == "qwen-test"


def test_groq_requires_api_key(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.delenv(
        "GROQ_API_KEY",
        raising=False,
    )

    messages = [
        SystemMessage(content="Rules"),
        HumanMessage(content="Question"),
    ]

    with pytest.raises(
        RuntimeError,
        match="GROQ_API_KEY",
    ):
        generate_with_groq(
            messages,
            "qwen-test",
            CONFIG,
        )
