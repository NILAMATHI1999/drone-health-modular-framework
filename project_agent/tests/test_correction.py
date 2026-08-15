from langchain_core.messages import HumanMessage

from app.correction import build_repair_prompt
from app.output_validator import (
    Severity,
    ValidationIssue,
    ValidationReport,
)


def test_repair_prompt_contains_failed_answer_and_errors() -> None:
    report = ValidationReport(
        issues=[
            ValidationIssue(
                Severity.ERROR,
                "missing_adaptation_manifest",
                "manifest missing",
            )
        ]
    )

    prompt = build_repair_prompt(
        [HumanMessage(content="adapt this node")],
        "FAILED ANSWER",
        report,
    )

    system_content = str(prompt.messages[0].content)
    human_content = str(prompt.messages[1].content)

    assert "complete files" in system_content
    assert "adaptation_manifest.yaml" in system_content
    assert "FAILED ANSWER" in human_content
    assert "missing_adaptation_manifest" in human_content
    assert "Return the complete corrected" in human_content
