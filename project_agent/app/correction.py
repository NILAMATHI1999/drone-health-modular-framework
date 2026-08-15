from dataclasses import dataclass

from langchain_core.messages import BaseMessage, HumanMessage, SystemMessage

from app.output_validator import ValidationReport, format_validation_report


SYSTEM_RULES = """You repair Drone Health Framework adaptation outputs.

Fix only the failed or missing parts from validation.
Return the full corrected answer again, not only a patch.

Rules:
- Return complete files, not snippets.
- Include complete CMakeLists.txt.
- Include complete package.xml.
- Include adaptation_manifest.yaml.
- Do not use top-level validator: in adaptation_manifest.yaml.
- Use topics.owned_outputs[].name, not topic.
- Use topics.heartbeat.name, not topic.
- files.source, files.yaml, files.build, files.dependencies, files.launch must be lists.
"""


@dataclass(frozen=True)
class RepairPrompt:
    messages: list[BaseMessage]


def build_repair_prompt(
    original_messages: list[BaseMessage],
    failed_answer: str,
    report: ValidationReport,
) -> RepairPrompt:
    original_context = "\n\n".join(
        str(message.content)
        for message in original_messages
    )

    human_content = "\n\n".join(
        [
            "ORIGINAL TASK AND PROJECT CONTEXT:",
            original_context,
            "FAILED GENERATED ANSWER:",
            failed_answer,
            "VALIDATION ERRORS:",
            format_validation_report(report),
            "Return the complete corrected adaptation answer.",
        ]
    )

    return RepairPrompt(
        messages=[
            SystemMessage(content=SYSTEM_RULES),
            HumanMessage(content=human_content),
        ]
    )
