import json
from collections.abc import Iterator
from typing import Any
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

from langchain_core.messages import BaseMessage

from app.generation import message_text


def stream_ollama(
    messages: list[BaseMessage],
    model: str,
    config: dict[str, Any],
    timeout_s: int = 600,
) -> Iterator[str]:
    base_url = (
        config["providers"]["ollama"]["base_url"]
        .rstrip("/")
    )
    generation = config["generation"]

    payload = {
        "model": model,
        "stream": True,
        "messages": [
            {
                "role": (
                    "system"
                    if message.type == "system"
                    else "user"
                ),
                "content": message_text(message),
            }
            for message in messages
        ],
        "options": {
            "temperature": generation["temperature"],
            "num_ctx": generation["context_window"],
            "num_predict": (
                generation["max_output_tokens"]
            ),
        },
    }

    request = Request(
        f"{base_url}/api/chat",
        data=json.dumps(payload).encode("utf-8"),
        headers={
            "Content-Type": "application/json",
        },
        method="POST",
    )

    try:
        with urlopen(
            request,
            timeout=timeout_s,
        ) as response:
            for raw_line in response:
                if not raw_line.strip():
                    continue

                event = json.loads(
                    raw_line.decode("utf-8")
                )

                message = event.get(
                    "message",
                    {},
                )
                token = message.get(
                    "content",
                    "",
                )

                if token:
                    yield token

                if event.get("done", False):
                    break

    except HTTPError as error:
        details = error.read().decode(
            "utf-8",
            errors="replace",
        )
        raise RuntimeError(
            f"Ollama request failed "
            f"({error.code}): {details}"
        ) from error
    except (URLError, TimeoutError) as error:
        raise RuntimeError(
            f"Ollama is unavailable or timed out: "
            f"{error}"
        ) from error
