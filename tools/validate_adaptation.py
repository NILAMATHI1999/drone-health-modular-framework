
import argparse
import ast
import re
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import yaml


@dataclass
class Issue:
    severity: str
    code: str
    message: str


class Validator:
    def __init__(self, root: Path, manifest: dict[str, Any]) -> None:
        self.root = root
        self.manifest = manifest
        self.issues: list[Issue] = []
        self.loaded_files: dict[str, list[Path]] = {}

    def error(self, code: str, message: str) -> None:
        self.issues.append(Issue("ERROR", code, message))

    def warning(self, code: str, message: str) -> None:
        self.issues.append(Issue("WARNING", code, message))

    @property
    def errors(self) -> list[Issue]:
        return [issue for issue in self.issues if issue.severity == "ERROR"]

    def report(self) -> int:
        print("\nGeneric Drone Health adaptation validation\n")
        print(f"Profile: {self.manifest.get('profile', 'missing')}")
        print(f"Root: {self.root}\n")

        for issue in self.issues:
            print(
                f"[{issue.severity}] {issue.code}: "
                f"{issue.message}"
            )

        if self.errors:
            print(
                f"\nValidation: FAIL "
                f"({len(self.errors)} errors)"
            )
            return 1

        warnings = len(self.issues)
        print(f"\nValidation: PASS ({warnings} warnings)")
        return 0

    def validate(self) -> int:
        self.validate_manifest()
        self.validate_expected_files()

        if self.errors:
            return self.report()

        self.validate_sources()
        self.validate_ros_yaml()
        self.validate_dependencies()
        self.validate_build_files()
        self.validate_launch_files()
        self.validate_cross_file_contract()

        return self.report()

    def validate_manifest(self) -> None:
        required_sections = (
            "profile",
            "ros_distribution",
            "node",
            "topics",
            "services",
            "files",
        )

        for section in required_sections:
            if section not in self.manifest:
                self.error(
                    "manifest_missing_section",
                    f"Manifest is missing: {section}",
                )

        node = self.manifest.get("node", {})

        for field in (
            "name",
            "module_name",
            "language",
            "lifecycle",
        ):
            if field not in node:
                self.error(
                    "manifest_missing_node_field",
                    f"Manifest node section is missing: {field}",
                )

        language = node.get("language")

        if language not in ("cpp", "python"):
            self.error(
                "manifest_language",
                "node.language must be cpp or python.",
            )

        topics = self.manifest.get("topics", {})

        if "owned_outputs" not in topics:
            self.error(
                "manifest_owned_outputs",
                "Manifest must define topics.owned_outputs.",
            )

        heartbeat = topics.get("heartbeat")

        if not isinstance(heartbeat, dict):
            self.error(
                "manifest_heartbeat",
                "Manifest must define topics.heartbeat.",
            )
        else:
            for field in (
                "name",
                "message_type",
                "period_ms",
                "deadline_ms",
                "liveliness_ms",
            ):
                if field not in heartbeat:
                    self.error(
                        "manifest_heartbeat_field",
                        f"Heartbeat is missing: {field}",
                    )

            period = heartbeat.get("period_ms")
            deadline = heartbeat.get("deadline_ms")

            if (
                isinstance(period, int)
                and isinstance(deadline, int)
                and deadline <= period
            ):
                self.error(
                    "manifest_heartbeat_deadline",
                    "Heartbeat deadline must exceed its period.",
                )

        services = self.manifest.get("services", {})

        expected_services = {
            "register": "/management/register_module",
            "deregister": "/management/deregister_module",
        }

        for field, expected in expected_services.items():
            if services.get(field) != expected:
                self.error(
                    "manifest_service",
                    f"services.{field} must be {expected}.",
                )

    def validate_expected_files(self) -> None:
        files = self.manifest.get("files", {})

        for category in (
            "source",
            "yaml",
            "build",
            "dependencies",
            "launch",
        ):
            configured = files.get(category, [])

            if not isinstance(configured, list):
                self.error(
                    "manifest_file_list",
                    f"files.{category} must be a list.",
                )
                continue

            self.loaded_files[category] = []

            for relative_name in configured:
                path = self.root / relative_name

                if not path.is_file():
                    self.error(
                        "missing_expected_file",
                        f"Missing {category} file: {relative_name}",
                    )
                    continue

                self.loaded_files[category].append(path)

    def combined_source(self) -> str:
        return "\n".join(
            path.read_text(errors="replace")
            for path in self.loaded_files.get("source", [])
        )

    def validate_sources(self) -> None:
        language = self.manifest["node"]["language"]

        for path in self.loaded_files.get("source", []):
            text = path.read_text(errors="replace")

            if language == "python":
                self.validate_python_source(path, text)
            else:
                self.validate_cpp_source(path, text)

        source = self.combined_source()

        required_text = {
            "RegisterModule": "RegisterModule integration is missing.",
            "DeregisterModule": "DeregisterModule integration is missing.",
            "MonitorSpec": "MonitorSpec integration is missing.",
            "/management/register_module": (
                "Correct Management register service is missing."
            ),
            "/management/deregister_module": (
                "Correct Management deregister service is missing."
            ),
            "request_deregister": (
                "Planned deregistration Trigger integration is missing."
            ),
            "heartbeat": "Heartbeat integration is missing.",
        }

        for required, message in required_text.items():
            if required not in source:
                self.error(
                    "missing_integration_content",
                    message,
                )

        if re.search(
            r"(?<!/management)/register_module",
            source,
        ):
            self.error(
                "wrong_register_service",
                "Found a register service outside /management.",
            )

        if re.search(
            r"(?<!/management)/deregister_module",
            source,
        ):
            self.error(
                "wrong_deregister_service",
                "Found a deregister service outside /management.",
            )

        self.validate_topic_ownership(source)
        self.validate_lifecycle_rules(source)

    def validate_python_source(
        self,
        path: Path,
        text: str,
    ) -> None:
        try:
            ast.parse(text, filename=str(path))
        except SyntaxError as error:
            self.error(
                "python_syntax",
                f"{path}: {error}",
            )

        if "Duration(milliseconds=" in text:
            self.error(
                "unsupported_duration_milliseconds",
                (
                    "ROS 2 Jazzy rclpy Duration does not accept "
                    "milliseconds=. Use Duration(nanoseconds="
                    "value_ms * 1_000_000)."
                ),
            )

        if (
            "heartbeat_liveliness_ms" in text
            and "QoSLivelinessPolicy.MANUAL_BY_TOPIC" not in text
        ):
            self.error(
                "missing_python_liveliness_policy",
                "Heartbeat liveliness parameter exists but "
                "MANUAL_BY_TOPIC QoS is missing.",
            )

        if (
            "heartbeat_liveliness_ms" in text
            and "liveliness_lease_duration" not in text
        ):
            self.error(
                "missing_python_liveliness_lease",
                "Heartbeat liveliness lease is not applied.",
            )

    def validate_cpp_source(
        self,
        path: Path,
        text: str,
    ) -> None:
        if "assert_liveliness();" in text:
            self.warning(
                "unchecked_assert_liveliness",
                (
                    f"{path}: verify that assert_liveliness() "
                    "return value is checked, especially under -Werror."
                ),
            )

        if "request->monitor_spec" in text:
            self.error(
                "invalid_monitor_field",
                f"{path}: use request->monitors, not monitor_spec.",
            )

        if re.search(r"\bmonitor_spec_?\.name\b", text):
            self.error(
                "invalid_monitor_name",
                f"{path}: MonitorSpec uses topic_name, not name.",
            )

        if re.search(r"\bmonitor_spec_?\.type\b", text):
            self.error(
                "invalid_monitor_type",
                f"{path}: MonitorSpec uses message_type, not type.",
            )

    def validate_topic_ownership(self, source: str) -> None:
        topics = self.manifest["topics"]
        register_function = self.extract_registration_region(source)

        for item in topics.get("external_inputs", []):
            name = item.get("name")

            if name and name in register_function:
                self.error(
                    "registered_external_input",
                    (
                        f"External input {name} appears in the "
                        "registration request."
                    ),
                )

        for item in topics.get("owned_outputs", []):
            name = item.get("name")
            message_type = item.get("message_type")

            if name and name not in source:
                self.error(
                    "missing_owned_output",
                    f"Owned output is missing from source: {name}",
                )

            if message_type and message_type not in source:
                self.error(
                    "missing_output_message_type",
                    (
                        "Owned output message type is missing: "
                        f"{message_type}"
                    ),
                )

        heartbeat = topics.get("heartbeat", {})
        heartbeat_name = heartbeat.get("name")

        if heartbeat_name and heartbeat_name not in source:
            self.error(
                "missing_heartbeat_topic",
                f"Heartbeat topic is missing: {heartbeat_name}",
            )

    def extract_registration_region(self, source: str) -> str:
        function_names = (
            "_request_register",
            "request_register",
            "register_module",
        )

        if self.manifest["node"]["language"] == "python":
            try:
                tree = ast.parse(source)
            except SyntaxError:
                return source

            for node in ast.walk(tree):
                if (
                    isinstance(node, ast.FunctionDef)
                    and node.name in function_names
                ):
                    lines = source.splitlines()
                    return "\n".join(
                        lines[node.lineno - 1 : node.end_lineno]
                    )

        matches = re.search(
            r"(?:void\s+)?request_register\s*\([^)]*\)\s*\{"
            r"(?P<body>.*?)\n\s*\}",
            source,
            re.DOTALL,
        )

        if matches:
            return matches.group("body")

        return source

    def validate_lifecycle_rules(self, source: str) -> None:
        lifecycle = self.manifest["node"].get(
            "lifecycle",
            False,
        )

        if not lifecycle:
            return

        required_callbacks = (
            "on_configure",
            "on_activate",
            "on_deactivate",
            "on_cleanup",
            "on_shutdown",
        )

        for callback in required_callbacks:
            if callback not in source:
                self.error(
                    "missing_lifecycle_callback",
                    f"Lifecycle callback is missing: {callback}",
                )

        if self.manifest["node"]["language"] == "python":
            try:
                tree = ast.parse(source)
            except SyntaxError:
                return

            deactivate = self.python_function_text(
                tree,
                source,
                "on_deactivate",
            )

            for forbidden in (
                "_request_deregister(",
                "_deregister_requested",
                "DeregisterModule",
            ):
                if forbidden in deactivate:
                    self.error(
                        "deregister_on_deactivate",
                        (
                            "Normal on_deactivate() must not "
                            f"contain {forbidden}."
                        ),
                    )

            response_callback = self.python_function_text(
                tree,
                source,
                "_handle_deregister_response",
            )

            if "rclpy.shutdown()" in response_callback:
                self.error(
                    "shutdown_in_async_callback",
                    "Do not call rclpy.shutdown() directly "
                    "inside the async response callback.",
                )

    def python_function_text(
        self,
        tree: ast.AST,
        source: str,
        name: str,
    ) -> str:
        lines = source.splitlines()

        for node in ast.walk(tree):
            if (
                isinstance(node, ast.FunctionDef)
                and node.name == name
            ):
                return "\n".join(
                    lines[node.lineno - 1 : node.end_lineno]
                )

        return ""

    def validate_ros_yaml(self) -> None:
        yaml_files = self.loaded_files.get("yaml", [])

        if not yaml_files:
            self.error(
                "missing_ros_yaml",
                "No ROS parameter YAML file was configured.",
            )
            return

        for path in yaml_files:
            try:
                data = yaml.safe_load(path.read_text())
            except yaml.YAMLError as error:
                self.error(
                    "invalid_ros_yaml",
                    f"{path}: {error}",
                )
                continue

            parameters = self.find_ros_parameters(data)

            if parameters is None:
                self.error(
                    "missing_ros_parameters",
                    f"{path}: ros__parameters is missing.",
                )
                continue

            self.validate_yaml_contract(path, parameters)

    def find_ros_parameters(
        self,
        value: Any,
    ) -> dict[str, Any] | None:
        if not isinstance(value, dict):
            return None

        parameters = value.get("ros__parameters")

        if isinstance(parameters, dict):
            return parameters

        for nested in value.values():
            result = self.find_ros_parameters(nested)

            if result is not None:
                return result

        return None

    def validate_yaml_contract(
        self,
        path: Path,
        parameters: dict[str, Any],
    ) -> None:
        heartbeat = self.manifest["topics"]["heartbeat"]
        services = self.manifest["services"]

        expected = {
            "module_name": self.manifest["node"]["module_name"],
            "heartbeat_topic": heartbeat["name"],
            "heartbeat_deadline_ms": heartbeat["deadline_ms"],
            "heartbeat_liveliness_ms": heartbeat["liveliness_ms"],
            "request_deregister_service": (
                services["request_deregister"]
            ),
        }

        for name, value in expected.items():
            if name not in parameters:
                self.error(
                    "missing_yaml_parameter",
                    f"{path}: missing parameter {name}.",
                )
            elif parameters[name] != value:
                self.error(
                    "yaml_manifest_mismatch",
                    (
                        f"{path}: {name}={parameters[name]!r}, "
                        f"expected {value!r}."
                    ),
                )

        period_names = (
            "heartbeat_period_ms",
            "publish_period_ms",
        )

        period = next(
            (
                parameters[name]
                for name in period_names
                if isinstance(parameters.get(name), int)
            ),
            heartbeat["period_ms"],
        )

        deadline = parameters.get(
            "heartbeat_deadline_ms",
        )

        if (
            isinstance(period, int)
            and isinstance(deadline, int)
            and deadline <= period
        ):
            self.error(
                "unsafe_heartbeat_deadline",
                (
                    f"{path}: heartbeat deadline "
                    "must exceed heartbeat period."
                ),
            )

        for output in self.manifest["topics"].get(
            "owned_outputs",
            [],
        ):
            output_name = output["name"]
            output_deadline = output.get("deadline_ms")

            if output_name not in str(parameters):
                self.warning(
                    "output_topic_parameter_not_found",
                    (
                        f"{path}: could not confirm an output "
                        f"parameter for {output_name}."
                    ),
                )

            if (
                not isinstance(output_deadline, int)
                or output_deadline <= 0
            ):
                self.error(
                    "invalid_manifest_output_deadline",
                    (
                        f"Invalid output deadline for "
                        f"{output_name}."
                    ),
                )

    def validate_dependencies(self) -> None:
        for path in self.loaded_files.get(
            "dependencies",
            [],
        ):
            try:
                tree = ET.parse(path)
            except ET.ParseError as error:
                self.error(
                    "invalid_package_xml",
                    f"{path}: {error}",
                )
                continue

            dependencies = {
                element.text.strip()
                for element in tree.getroot()
                if element.tag.endswith("depend")
                and element.text
            }

            required = {
                "drone_health_interfaces",
                "std_msgs",
                "std_srvs",
            }

            language = self.manifest["node"]["language"]
            required.add(
                "rclpy" if language == "python" else "rclcpp"
            )

            for dependency in required:
                if dependency not in dependencies:
                    self.error(
                        "missing_package_dependency",
                        (
                            f"{path}: missing dependency "
                            f"{dependency}."
                        ),
                    )

    def validate_build_files(self) -> None:
        language = self.manifest["node"]["language"]

        for path in self.loaded_files.get("build", []):
            text = path.read_text(errors="replace")

            if language == "python":
                for yaml_path in self.loaded_files.get(
                    "yaml",
                    [],
                ):
                    if yaml_path.name not in text:
                        self.error(
                            "yaml_not_installed",
                            (
                                f"{path}: does not install "
                                f"{yaml_path.name}."
                            ),
                        )
            else:
                if "drone_health_interfaces" not in text:
                    self.error(
                        "missing_cmake_dependency",
                        (
                            f"{path}: missing "
                            "drone_health_interfaces."
                        ),
                    )

                if "install(TARGETS" not in text:
                    self.warning(
                        "target_install_not_confirmed",
                        f"{path}: target installation not found.",
                    )

    def validate_launch_files(self) -> None:
        for path in self.loaded_files.get("launch", []):
            text = path.read_text(errors="replace")

            if path.suffix == ".py":
                try:
                    ast.parse(text, filename=str(path))
                except SyntaxError as error:
                    self.error(
                        "launch_python_syntax",
                        f"{path}: {error}",
                    )

        if self.loaded_files.get("yaml"):
            yaml_names = {
                path.name
                for path in self.loaded_files["yaml"]
            }

            all_launch_text = "\n".join(
                path.read_text(errors="replace")
                for path in self.loaded_files.get(
                    "launch",
                    [],
                )
            )

            if not any(
                name in all_launch_text
                for name in yaml_names
            ):
                self.warning(
                    "yaml_not_loaded_by_launch",
                    "No launch file appears to load the ROS YAML.",
                )

    def validate_cross_file_contract(self) -> None:
        source = self.combined_source()
        all_text = source + "\n" + "\n".join(
            path.read_text(errors="replace")
            for paths in self.loaded_files.values()
            for path in paths
        )

        for output in self.manifest["topics"].get(
            "owned_outputs",
            [],
        ):
            if output["message_type"] not in all_text:
                self.error(
                    "output_type_contract",
                    (
                        "Expected output message type is missing: "
                        f"{output['message_type']}"
                    ),
                )

        external_names = {
            item["name"]
            for item in self.manifest["topics"].get(
                "external_inputs",
                [],
            )
        }

        owned_names = {
            item["name"]
            for item in self.manifest["topics"].get(
                "owned_outputs",
                [],
            )
        }

        overlap = external_names & owned_names

        if overlap:
            self.error(
                "manifest_topic_ownership_overlap",
                (
                    "Topics cannot be both external and owned: "
                    f"{sorted(overlap)}"
                ),
            )


def load_manifest(path: Path) -> dict[str, Any]:
    try:
        data = yaml.safe_load(path.read_text())
    except (OSError, yaml.YAMLError) as error:
        raise ValueError(
            f"Cannot load manifest: {error}"
        ) from error

    if not isinstance(data, dict):
        raise ValueError(
            "Manifest root must be a YAML mapping."
        )

    return data


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Validate a generated Drone Health adaptation "
            "against a manifest."
        )
    )
    parser.add_argument(
        "adaptation_directory",
        type=Path,
        help="Root directory containing adapted files.",
    )
    parser.add_argument(
        "--manifest",
        required=True,
        type=Path,
        help="Adaptation manifest YAML.",
    )
    args = parser.parse_args()

    root = args.adaptation_directory.expanduser().resolve()
    manifest_path = args.manifest.expanduser().resolve()

    if not root.is_dir():
        print(
            f"Adaptation directory does not exist: {root}",
            file=sys.stderr,
        )
        return 2

    try:
        manifest = load_manifest(manifest_path)
    except ValueError as error:
        print(error, file=sys.stderr)
        return 2

    return Validator(root, manifest).validate()


if __name__ == "__main__":
    sys.exit(main())
