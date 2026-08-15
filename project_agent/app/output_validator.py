
import re
from dataclasses import dataclass
from enum import StrEnum
from typing import Any

import yaml


class Severity(StrEnum):
    ERROR = "error"
    WARNING = "warning"


@dataclass(frozen=True)
class ValidationIssue:
    severity: Severity
    code: str
    message: str


@dataclass(frozen=True)
class ValidationReport:
    issues: list[ValidationIssue]

    @property
    def passed(self) -> bool:
        return not any(
            issue.severity is Severity.ERROR
            for issue in self.issues
        )


REQUIRED_ADAPTATION_TEXT = {
    "management_register_service": (
        "/management/register_module"
    ),
    "management_deregister_service": (
        "/management/deregister_module"
    ),
    "monitor_topic_name": "topic_name",
    "monitor_kind": "kind",
    "monitor_message_type": "message_type",
    "monitor_array": "request->monitors",
    "heartbeat": "heartbeat",
    "ros_parameters": "ros__parameters",
    "deregister_trigger": "request_deregister",
}

FORBIDDEN_PATTERNS = {
    "invented_monitor_name": (
        r"\bmonitor_spec_?\.name\b"
    ),
    "invented_monitor_type": (
        r"\bmonitor_spec_?\.type\b"
    ),
    "singular_monitor_request": (
        r"\brequest->monitor_spec\b"
    ),
}


def extract_fenced_blocks(
    answer: str,
    language: str,
) -> list[str]:
    pattern = re.compile(
        rf"```{re.escape(language)}\s*\n"
        r"(.*?)```",
        re.DOTALL | re.IGNORECASE,
    )

    return [
        match.strip()
        for match in pattern.findall(answer)
    ]


def find_parameter_mapping(
    value: Any,
) -> dict[str, Any] | None:
    if not isinstance(value, dict):
        return None

    if "ros__parameters" in value:
        parameters = value["ros__parameters"]

        if isinstance(parameters, dict):
            return parameters

        return None

    for nested_value in value.values():
        result = find_parameter_mapping(
            nested_value
        )

        if result is not None:
            return result

    return None


def validate_yaml(
    answer: str,
) -> list[ValidationIssue]:
    issues: list[ValidationIssue] = []
    blocks = extract_fenced_blocks(
        answer,
        "yaml",
    )

    if not blocks:
        return [
            ValidationIssue(
                Severity.ERROR,
                "missing_yaml",
                "No fenced YAML configuration was generated.",
            )
        ]

    try:
        yaml_data = yaml.safe_load(blocks[0])
    except yaml.YAMLError as error:
        return [
            ValidationIssue(
                Severity.ERROR,
                "invalid_yaml",
                f"Generated YAML is invalid: {error}",
            )
        ]

    parameters = find_parameter_mapping(
        yaml_data
    )

    if parameters is None:
        issues.append(
            ValidationIssue(
                Severity.ERROR,
                "missing_ros_parameters",
                (
                    "Generated YAML does not contain "
                    "ros__parameters."
                ),
            )
        )

        return issues

    publish_period = parameters.get(
        "publish_period_ms"
    )

    if isinstance(publish_period, int):
        for name, value in parameters.items():
            if not (
                name.endswith("_deadline_ms")
                and isinstance(value, int)
            ):
                continue

            if value <= publish_period:
                issues.append(
                    ValidationIssue(
                        Severity.ERROR,
                        "unsafe_deadline",
                        (
                            f"{name} must be greater than "
                            f"publish_period_ms "
                            f"({value} <= {publish_period})."
                        ),
                    )
                )

    if (
        "request_deregister_service"
        not in parameters
    ):
        issues.append(
            ValidationIssue(
                Severity.ERROR,
                "missing_deregister_parameter",
                (
                    "YAML is missing "
                    "request_deregister_service."
                ),
            )
        )

    return issues

def validate_manifest_block(
    answer: str,
) -> list[ValidationIssue]:
    issues: list[ValidationIssue] = []
    blocks = extract_fenced_blocks(
        answer,
        "yaml",
    )

    manifest = None

    for block in blocks:
        try:
            data = yaml.safe_load(block)
        except yaml.YAMLError:
            continue

        if (
            isinstance(data, dict)
            and "profile" in data
            and "topics" in data
            and "files" in data
        ):
            manifest = data
            break

    if manifest is None:
        issues.append(
            ValidationIssue(
                Severity.ERROR,
                "missing_adaptation_manifest",
                (
                    "No adaptation_manifest.yaml block using "
                    "the required validator schema was generated."
                ),
            )
        )
        return issues

    if "validator" in manifest:
        issues.append(
            ValidationIssue(
                Severity.ERROR,
                "manifest_validator_key",
                (
                    "adaptation_manifest.yaml must not contain "
                    "a top-level validator key."
                ),
            )
        )

    for section in (
        "profile",
        "ros_distribution",
        "node",
        "topics",
        "services",
        "files",
    ):
        if section not in manifest:
            issues.append(
                ValidationIssue(
                    Severity.ERROR,
                    "manifest_missing_section",
                    f"adaptation_manifest.yaml is missing {section}.",
                )
            )

    topics = manifest.get("topics", {})
    if not isinstance(topics, dict):
        topics = {}

    if "owned_outputs" not in topics:
        issues.append(
            ValidationIssue(
                Severity.ERROR,
                "manifest_missing_owned_outputs",
                "Manifest must define topics.owned_outputs.",
            )
        )
    else:
        for output in topics.get("owned_outputs", []):
            if (
                isinstance(output, dict)
                and "topic" in output
            ):
                issues.append(
                    ValidationIssue(
                        Severity.ERROR,
                        "manifest_output_uses_topic",
                        (
                            "Use topics.owned_outputs[].name, "
                            "not topic."
                        ),
                    )
                )

    heartbeat = topics.get("heartbeat")
    if not isinstance(heartbeat, dict):
        issues.append(
            ValidationIssue(
                Severity.ERROR,
                "manifest_missing_heartbeat",
                "Manifest must define topics.heartbeat.",
            )
        )
    elif "topic" in heartbeat:
        issues.append(
            ValidationIssue(
                Severity.ERROR,
                "manifest_heartbeat_uses_topic",
                "Use topics.heartbeat.name, not topic.",
            )
        )

    files = manifest.get("files", {})
    if not isinstance(files, dict):
        files = {}

    for category in (
        "source",
        "yaml",
        "build",
        "dependencies",
        "launch",
    ):
        if not isinstance(files.get(category), list):
            issues.append(
                ValidationIssue(
                    Severity.ERROR,
                    "manifest_file_list",
                    f"files.{category} must be a list.",
                )
            )

    return issues


def validate_adaptation(
    answer: str,
) -> ValidationReport:
    issues: list[ValidationIssue] = []

    for code, required_text in (
        REQUIRED_ADAPTATION_TEXT.items()
    ):
        if required_text not in answer:
            issues.append(
                ValidationIssue(
                    Severity.ERROR,
                    f"missing_{code}",
                    (
                        "Required adaptation content "
                        f"is missing: {required_text}"
                    ),
                )
            )

    for code, pattern in (
        FORBIDDEN_PATTERNS.items()
    ):
        if re.search(pattern, answer):
            issues.append(
                ValidationIssue(
                    Severity.ERROR,
                    code,
                    (
                        "Generated output contains an "
                        f"invalid framework pattern: {pattern}"
                    ),
                )
            )

    if not extract_fenced_blocks(
        answer,
        "cpp",
    ):
        issues.append(
            ValidationIssue(
                Severity.ERROR,
                "missing_cpp",
                "No fenced C++ node was generated.",
            )
        )

    if "ament_target_dependencies" not in answer:
        issues.append(
            ValidationIssue(
                Severity.ERROR,
                "missing_cmake_dependencies",
                (
                    "CMake dependency additions "
                    "are missing."
                ),
            )
        )

    if "CMakeLists_additions.txt" in answer:
        issues.append(
            ValidationIssue(
                Severity.ERROR,
                "cmake_snippet_file",
                (
                    "Return a complete CMakeLists.txt, not "
                    "CMakeLists_additions.txt."
                ),
            )
        )

    if "package_xml_dependencies.txt" in answer:
        issues.append(
            ValidationIssue(
                Severity.ERROR,
                "package_snippet_file",
                (
                    "Return a complete package.xml, not "
                    "package_xml_dependencies.txt."
                ),
            )
        )

    if "<depend>" not in answer:
        issues.append(
            ValidationIssue(
                Severity.WARNING,
                "package_dependency_format",
                (
                    "No unified <depend> entry was found "
                    "for package.xml dependencies."
                ),
            )
        )

    issues.extend(
        validate_yaml(answer)
    )
    issues.extend(
        validate_manifest_block(answer)
    )

    return ValidationReport(issues=issues)


def format_validation_report(
    report: ValidationReport,
) -> str:
    status = (
        "PASS"
        if report.passed
        else "FAIL"
    )

    lines = [
        f"Validation: {status}",
    ]

    if not report.issues:
        lines.append(
            "No deterministic validation issues found."
        )

        return "\n".join(lines)

    for issue in report.issues:
        lines.append(
            f"- [{issue.severity.value}] "
            f"{issue.code}: {issue.message}"
        )

    return "\n".join(lines)
