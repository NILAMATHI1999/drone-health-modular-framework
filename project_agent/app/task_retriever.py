from dataclasses import dataclass

from app.augmentation import TaskMode
from app.chunker import chunk_documents
from app.hybrid_retriever import HybridRetriever
from app.loader import load_project_documents
from app.retriever import RetrievalResult


TEMPLATE_ROOT = (
    "src/drone_health_registrable_template"
)

TEMPLATE_SOURCES = {
    "publisher": {
        "cpp": (
            f"{TEMPLATE_ROOT}/template_node/"
            "registrable_publisher_template_node.cpp"
        ),
        "yaml": (
            f"{TEMPLATE_ROOT}/config/"
            "registrable_publisher_template.yaml"
        ),
    },
    "subscriber": {
        "cpp": (
            f"{TEMPLATE_ROOT}/template_node/"
            "registrable_subscriber_template_node.cpp"
        ),
        "yaml": (
            f"{TEMPLATE_ROOT}/config/"
            "registrable_subscriber_template.yaml"
        ),
    },
    "service": {
        "cpp": (
            f"{TEMPLATE_ROOT}/template_node/"
            "registrable_service_template_node.cpp"
        ),
        "yaml": (
            f"{TEMPLATE_ROOT}/config/"
            "registrable_service_template.yaml"
        ),
    },
}

SHARED_SOURCES = {
    "cmake": f"{TEMPLATE_ROOT}/CMakeLists.txt",
    "package": f"{TEMPLATE_ROOT}/package.xml",
    "readme": f"{TEMPLATE_ROOT}/README.md",
    "template_readme": f"{TEMPLATE_ROOT}/template_node/README.md",
}

EXPLANATION_SOURCES = {
    "management": "src/drone_health_core/management/README.md",
    "core": "src/drone_health_core/README.md",
    "camera_example": (
        "src/drone_health_examples/simulated_camera/README.md"
    ),
}


@dataclass(frozen=True)
class ChunkRule:
    source: str
    terms: tuple[str, ...]


def detect_template_type(user_input: str) -> str:
    text = user_input.lower()

    has_publisher = (
        "create_publisher" in text
        or "publisher_" in text
    )
    has_subscription = (
        "create_subscription" in text
        or "subscription_" in text
    )
    has_service = (
        "create_service" in text
        or "service callback" in text
    )

    if has_service and not has_publisher:
        return "service"

    if has_subscription:
        return "subscriber"

    return "publisher"

def rules_for_template(
    template_type: str,
) -> list[ChunkRule]:
    sources = TEMPLATE_SOURCES[template_type]
    cpp = sources["cpp"]

    return [
        ChunkRule(
            cpp,
            (
                "create_client<RegisterModule>",
                "create_client<DeregisterModule>",
            ),
        ),
        ChunkRule(
            cpp,
            (
                "void timer_callback",
                "request_register",
            ),
        ),
        ChunkRule(
            cpp,
            (
                "void request_register",
                "MonitorSpec",
            ),
        ),
        ChunkRule(
            cpp,
            (
                "handle_request_deregister",
                "deregistration requested",
            ),
        ),
        ChunkRule(
            cpp,
            (
                "void request_deregister",
                "DeregisterModule",
            ),
        ),
        ChunkRule(
            sources["yaml"],
            ("ros__parameters",),
        ),
        ChunkRule(
            SHARED_SOURCES["cmake"],
            (
                "ament_target_dependencies",
                "add_executable",
            ),
        ),
    ]


def rules_for_explanation(
    user_input: str,
) -> list[ChunkRule]:
    text = user_input.lower()
    rules: list[ChunkRule] = []

    if "management" in text:
        rules.extend(
            [
                ChunkRule(
                    EXPLANATION_SOURCES["management"],
                    (
                        "register_module",
                        "deregister_module",
                        "/management/state",
                    ),
                ),
                ChunkRule(
                    EXPLANATION_SOURCES["core"],
                    (
                        "management",
                        "supervisor",
                    ),
                ),
            ]
        )

    if "heartbeat" in text or "liveliness" in text:
        rules.extend(
            [
                ChunkRule(
                    SHARED_SOURCES["readme"],
                    (
                        "heartbeat",
                        "HealthMonitor",
                        "Management",
                    ),
                ),
                ChunkRule(
                    SHARED_SOURCES["template_readme"],
                    (
                        "heartbeat",
                        "register",
                        "deregister",
                    ),
                ),
            ]
        )

    if (
        "adapt" in text
        or "publisher" in text
        or "files" in text
    ):
        rules.extend(
            [
                ChunkRule(
                    SHARED_SOURCES["readme"],
                    (
                        "standalone",
                        "adaptation_manifest.yaml",
                        "CMakeLists.txt",
                    ),
                ),
                ChunkRule(
                    SHARED_SOURCES["template_readme"],
                    (
                        "YAML",
                        "CMakeLists.txt",
                        "package.xml",
                    ),
                ),
            ]
        )

    return rules


class TaskAwareRetriever:
    def __init__(self) -> None:
        self.hybrid = HybridRetriever()
        self.chunks = chunk_documents(
            load_project_documents()
        )

    def find_rule_match(
        self,
        rule: ChunkRule,
    ) -> RetrievalResult | None:
        candidates = [
            chunk
            for chunk in self.chunks
            if chunk.metadata["source"] == rule.source
        ]

        if not candidates:
            return None

        ranked = sorted(
            candidates,
            key=lambda chunk: sum(
                term.lower()
                in chunk.page_content.lower()
                for term in rule.terms
            ),
            reverse=True,
        )

        best = ranked[0]
        match_count = sum(
            term.lower() in best.page_content.lower()
            for term in rule.terms
        )

        if match_count == 0:
            return None

        return RetrievalResult(
            document=best,
            score=float(match_count),
            method="task_pinned",
        )

    def retrieve(
        self,
        mode: TaskMode,
        user_input: str,
        limit: int = 12,
    ) -> list[RetrievalResult]:
        hybrid_results = (
            self.hybrid.retrieve_all_methods(
                user_input,
                limit=limit,
                pool_size=max(limit * 2, 10),
            )["relative_score"]
        )

        if mode is TaskMode.EXPLAIN_PROJECT:
            pinned_results = [
                result
                for rule in rules_for_explanation(
                    user_input
                )
                if (
                    result := self.find_rule_match(rule)
                )
                is not None
            ]

            return self.combine_results(
                pinned_results,
                hybrid_results,
                limit,
            )

        if mode is not TaskMode.ADAPT_NODE:
            return hybrid_results[:limit]

        template_type = detect_template_type(
            user_input
        )

        pinned_results = [
            result
            for rule in rules_for_template(
                template_type
            )
            if (
                result := self.find_rule_match(rule)
            )
            is not None
        ]

        return self.combine_results(
            pinned_results,
            hybrid_results,
            limit,
        )

    def combine_results(
        self,
        pinned_results: list[RetrievalResult],
        hybrid_results: list[RetrievalResult],
        limit: int,
    ) -> list[RetrievalResult]:
        combined: list[RetrievalResult] = []
        seen: set[tuple[str, int]] = set()

        for result in (
            pinned_results + hybrid_results
        ):
            identity = (
                result.document.metadata["source"],
                result.document.metadata["chunk_index"],
            )

            if identity in seen:
                continue

            seen.add(identity)
            combined.append(result)

            if len(combined) == limit:
                break

        return combined
