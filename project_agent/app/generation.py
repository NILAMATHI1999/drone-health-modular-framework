import json
import os
from dataclasses import dataclass
from enum import StrEnum
from pathlib import Path
from typing import Any
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

import yaml
from langchain_core.messages import BaseMessage

from app.augmentation import TaskMode


CONFIG_PATH = (
    Path(__file__).resolve().parents[1]
    / "config"
    / "models.yaml"
)


class Provider(StrEnum):
    OLLAMA = "ollama"
    GEMINI = "gemini"
    GROQ = "groq"



@dataclass(frozen=True)
class ModelSelection:
    provider: Provider
    model: str


@dataclass(frozen=True)
class GenerationResult:
    text: str
    provider: Provider
    model: str


def load_model_config(
    config_path: Path = CONFIG_PATH,
) -> dict[str, Any]:
    config = yaml.safe_load(config_path.read_text())

    if not isinstance(config, dict):
        raise ValueError(
            "model configuration must be a mapping"
        )

    return config


def select_model(
    mode: TaskMode,
    provider_override: Provider | None = None,
    model_override: str | None = None,
    config: dict[str, Any] | None = None,
) -> ModelSelection:
    selected_config = config or load_model_config()
    route = selected_config["routing"][mode.value]

    provider = (
        provider_override
        or Provider(route["provider"])
    )

    if provider_override:
        default_model = selected_config[
            "providers"
        ][provider.value].get("model")
    else:
        default_model = route["model"]

    model = model_override or default_model

    if not model:
        raise ValueError(
            f"no model configured for {provider.value}"
        )

    return ModelSelection(
        provider=provider,
        model=model,
    )



def message_text(message: BaseMessage) -> str:
    if isinstance(message.content, str):
        return message.content

    return json.dumps(message.content)


def post_json(
    url: str,
    payload: dict[str, Any],
    headers: dict[str, str] | None = None,
    timeout_s: int = 600,
) -> dict[str, Any]:
    request_headers = {
        "Content-Type": "application/json",
        "User-Agent": "drone-health-project-assistant/0.1",
    }
    request_headers.update(headers or {})

    request = Request(
        url,
        data=json.dumps(payload).encode("utf-8"),
        headers=request_headers,
        method="POST",
    )

    try:
        with urlopen(
            request,
            timeout=timeout_s,
        ) as response:
            return json.loads(
                response.read().decode("utf-8")
            )
    except HTTPError as error:
        details = error.read().decode(
            "utf-8",
            errors="replace",
        )
        raise RuntimeError(
            f"provider request failed "
            f"({error.code}): {details}"
        ) from error
    except URLError as error:
        raise RuntimeError(
            f"provider is unavailable: {error.reason}"
        ) from error


def generate_with_ollama(
    messages: list[BaseMessage],
    model: str,
    config: dict[str, Any],
) -> str:
    base_url = (
        config["providers"]["ollama"]["base_url"]
        .rstrip("/")
    )
    generation = config["generation"]

    payload = {
        "model": model,
        "stream": False,
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

    response = post_json(
        f"{base_url}/api/chat",
        payload,
    )

    try:
        return response["message"]["content"]
    except (KeyError, TypeError) as error:
        raise RuntimeError(
            "Ollama returned no generated text"
        ) from error


def generate_with_gemini(
    messages: list[BaseMessage],
    model: str,
    config: dict[str, Any],
) -> str:
    api_key = os.environ.get("GEMINI_API_KEY")

    if not api_key:
        raise RuntimeError(
            "GEMINI_API_KEY is required "
            "when Gemini is selected"
        )

    generation = config["generation"]

    system_text = "\n\n".join(
        message_text(message)
        for message in messages
        if message.type == "system"
    )

    user_text = "\n\n".join(
        message_text(message)
        for message in messages
        if message.type != "system"
    )

    payload = {
        "systemInstruction": {
            "parts": [
                {"text": system_text},
            ],
        },
        "contents": [
            {
                "role": "user",
                "parts": [
                    {"text": user_text},
                ],
            },
        ],
        "generationConfig": {
            "temperature": generation["temperature"],
            "maxOutputTokens": (
                generation["max_output_tokens"]
            ),
        },
    }

    url = (
        "https://generativelanguage.googleapis.com/"
        f"v1beta/models/{model}:generateContent"
    )

    response = post_json(
        url,
        payload,
        headers={
            "x-goog-api-key": api_key,
        },
    )

    try:
        parts = (
            response["candidates"][0]
            ["content"]["parts"]
        )
        return "".join(
            part.get("text", "")
            for part in parts
        ).strip()
    except (
        IndexError,
        KeyError,
        TypeError,
    ) as error:
        raise RuntimeError(
            "Gemini returned no generated text"
        ) from error
def generate_with_groq(
    messages: list[BaseMessage],
    model: str,
    config: dict[str, Any],
) -> str:
    api_key = os.environ.get("GROQ_API_KEY")

    if not api_key:
        raise RuntimeError(
            "GROQ_API_KEY is required "
            "when Groq is selected"
        )

    base_url = (
        config["providers"]["groq"]["base_url"]
        .rstrip("/")
    )
    generation = config["generation"]

    payload = {
        "model": model,
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
        "temperature": generation["temperature"],
        "max_completion_tokens": (
            generation["max_output_tokens"]
        ),
        "reasoning_effort": "none",
    }

    response = post_json(
        f"{base_url}/chat/completions",
        payload,
        headers={
            "Authorization": f"Bearer {api_key}",
        },
        timeout_s=180,
    )

    try:
        content = response["choices"][0][
            "message"
        ]["content"]

        if not content:
            raise ValueError("empty content")

        return content.strip()
    except (
        IndexError,
        KeyError,
        TypeError,
        ValueError,
    ) as error:
        raise RuntimeError(
            "Groq returned no generated text"
        ) from error



def generate_answer(
    messages: list[BaseMessage],
    selection: ModelSelection,
    config: dict[str, Any] | None = None,
) -> GenerationResult:
    selected_config = config or load_model_config()

    if selection.provider is Provider.OLLAMA:
        text = generate_with_ollama(
            messages,
            selection.model,
            selected_config,
        )
    elif selection.provider is Provider.GEMINI:
        text = generate_with_gemini(
            messages,
            selection.model,
            selected_config,
        )
    elif selection.provider is Provider.GROQ:
        text = generate_with_groq(
            messages,
            selection.model,
            selected_config,
        )




    else:
        raise ValueError(
            f"unsupported provider: "
            f"{selection.provider}"
        )

    return GenerationResult(
        text=text,
        provider=selection.provider,
        model=selection.model,
    )
