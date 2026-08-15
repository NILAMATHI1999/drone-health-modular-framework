from app.augmentation import TaskMode
from app.pipeline import ProjectRAGPipeline


class FakeRetriever:
    def __init__(self) -> None:
        self.calls: list[dict[str, object]] = []

    def retrieve(
        self,
        mode: TaskMode,
        user_input: str,
        limit: int,
    ) -> list:
        self.calls.append(
            {
                "mode": mode,
                "user_input": user_input,
                "limit": limit,
            }
        )

        return []


def test_adapt_node_uses_complete_template() -> None:
    pipeline = ProjectRAGPipeline()

    prepared = pipeline.prepare(
        mode=TaskMode.ADAPT_NODE,
        user_input=(
            'Node("battery_publisher") '
            'create_publisher<std_msgs::msg::Float32>'
        ),
    )

    content = str(
        prepared.messages[1].content
    )

    assert "void setup_qos()" in content
    assert "void request_register()" in content
    assert "request->monitors.push_back" in content
    assert "ros__parameters:" in content
    assert prepared.results == []


def test_adapt_node_does_not_initialize_retriever() -> None:
    pipeline = ProjectRAGPipeline()

    pipeline.prepare(
        mode=TaskMode.ADAPT_NODE,
        user_input="publisher node",
    )

    assert pipeline._retriever is None


def test_explanation_still_uses_retriever() -> None:
    retriever = FakeRetriever()

    pipeline = ProjectRAGPipeline(
        retriever=retriever,
    )

    pipeline.prepare(
        mode=TaskMode.EXPLAIN_PROJECT,
        user_input="Explain ManagementNode",
        max_chunks=3,
    )

    assert len(retriever.calls) == 1
    assert retriever.calls[0]["mode"] is (
        TaskMode.EXPLAIN_PROJECT
    )
    assert retriever.calls[0]["limit"] == 3


def test_reuse_still_uses_retriever() -> None:
    retriever = FakeRetriever()

    pipeline = ProjectRAGPipeline(
        retriever=retriever,
    )

    pipeline.prepare(
        mode=TaskMode.REUSE_COMPONENT,
        user_input="How can I reuse MonitorSpec?",
        max_chunks=4,
    )

    assert len(retriever.calls) == 1
    assert retriever.calls[0]["mode"] is (
        TaskMode.REUSE_COMPONENT
    )
    assert retriever.calls[0]["limit"] == 4
