import json
from typing import Any

from langchain_core.messages import (
    HumanMessage,
    SystemMessage,
)

from app.ollama_stream import stream_ollama


CONFIG = {
    "providers": {
        "ollama": {
            "base_url": "http://localhost:11434",
        },
    },
    "generation": {
        "temperature": 0.1,
        "context_window": 8192,
        "max_output_tokens": 2048,
    },
}


class FakeResponse:
    def __init__(
        self,
        events: list[dict[str, Any]],
    ) -> None:
        self.lines = [
            (
                json.dumps(event)
                + "\n"
            ).encode("utf-8")
            for event in events
        ]

    def __enter__(self) -> "FakeResponse":
        return self

    def __exit__(
        self,
        exception_type: object,
        exception: object,
        traceback: object,
    ) -> None:
        return None

    def __iter__(self):
        return iter(self.lines)


def test_stream_ollama_yields_tokens(
    monkeypatch,
) -> None:
    events = [
        {
            "message": {
                "content": "Hello",
            },
            "done": False,
        },
        {
            "message": {
                "content": " world",
            },
            "done": False,
        },
        {
            "message": {
                "content": "",
            },
            "done": True,
        },
    ]

    def fake_urlopen(
        request,
        timeout,
    ) -> FakeResponse:
        assert timeout == 600
        return FakeResponse(events)

    monkeypatch.setattr(
        "app.ollama_stream.urlopen",
        fake_urlopen,
    )

    messages = [
        SystemMessage(content="Rules"),
        HumanMessage(content="Question"),
    ]

    tokens = list(
        stream_ollama(
            messages,
            "qwen2.5-coder:3b",
            CONFIG,
        )
    )

    assert tokens == [
        "Hello",
        " world",
    ]
