import argparse
import ast
import sys
from pathlib import Path

import yaml


class Validator:
    def __init__(self) -> None:
        self.errors: list[str] = []
        self.warnings: list[str] = []

    def error(self, code: str, message: str) -> None:
        self.errors.append(f"[ERROR] {code}: {message}")

    def warning(self, code: str, message: str) -> None:
        self.warnings.append(f"[WARNING] {code}: {message}")

    def require_text(
        self,
        text: str,
        required: str,
        code: str,
        message: str,
    ) -> None:
        if required not in text:
            self.error(code, message)

    def report(self) -> int:
        print("\nYOLO Drone Health adaptation validation\n")

        for issue in self.errors:
            print(issue)

        for issue in self.warnings:
            print(issue)

        if self.errors:
            print(f"\nValidation: FAIL ({len(self.errors)} errors)")
            return 1

        print(f"\nValidation: PASS ({len(self.warnings)} warnings)")
        return 0


def find_file(root: Path, relative_suffix: str) -> Path | None:
    matches = [
        path
        for path in root.rglob("*")
        if path.is_file() and path.as_posix().endswith(relative_suffix)
    ]

    if len(matches) != 1:
        return None

    return matches[0]


def get_function(tree: ast.AST, name: str) -> ast.FunctionDef | None:
    for node in ast.walk(tree):
        if isinstance(node, ast.FunctionDef) and node.name == name:
            return node

    return None


def function_text(source: str, node: ast.FunctionDef | None) -> str:
    if node is None:
        return ""

    lines = source.splitlines()
    return "\n".join(lines[node.lineno - 1 : node.end_lineno])


def load_yaml_parameters(
    validator: Validator,
    yaml_path: Path,
) -> dict:
    try:
        data = yaml.safe_load(yaml_path.read_text())
    except (OSError, yaml.YAMLError) as error:
        validator.error("invalid_yaml", f"Cannot load YAML: {error}")
        return {}

    if not isinstance(data, dict) or len(data) != 1:
        validator.error(
            "yaml_top_level",
            "YAML must contain exactly one ROS node top-level key.",
        )
        return {}

    node_key, node_config = next(iter(data.items()))

    if node_key not in ("/yolo/yolo_node", "yolo_node"):
        validator.error(
            "yaml_node_key",
            f"Unexpected YAML node key: {node_key}",
        )

    if not isinstance(node_config, dict):
        validator.error("yaml_structure", "Node configuration must be a mapping.")
        return {}

    parameters = node_config.get("ros__parameters")

    if not isinstance(parameters, dict):
        validator.error(
            "missing_ros_parameters",
            "YAML must contain ros__parameters.",
        )
        return {}

    return parameters


def validate_python(
    validator: Validator,
    source_path: Path,
) -> None:
    source = source_path.read_text()

    try:
        tree = ast.parse(source, filename=str(source_path))
    except SyntaxError as error:
        validator.error("python_syntax", str(error))
        return

    required = {
        "RegisterModule": "RegisterModule is missing.",
        "DeregisterModule": "DeregisterModule is missing.",
        "MonitorSpec": "MonitorSpec is missing.",
        "/management/register_module": "Correct register service is missing.",
        "/management/deregister_module": "Correct deregister service is missing.",
        "request_deregister_service": "Deregister service parameter is missing.",
        "heartbeat_topic": "Heartbeat topic parameter is missing.",
        "output_deadline_ms": "Output deadline parameter is missing.",
        "yolo_msgs/msg/DetectionArray": "Detection MonitorSpec type is missing.",
    }

    for text, message in required.items():
        validator.require_text(
            source,
            text,
            f"missing_{text.replace('/', '_')}",
            message,
        )

    if "Duration(milliseconds=" in source:
        validator.error(
            "unsupported_duration_milliseconds",
            (
                "ROS 2 Jazzy rclpy Duration does not accept milliseconds=. "
                "Use Duration(nanoseconds=value_ms * 1_000_000)."
            ),
        )

    if '"/image_raw"' in function_text(
        source,
        get_function(tree, "_request_register"),
    ):
        validator.error(
            "registered_input_topic",
            "/image_raw must not be registered by YOLO.",
        )

    deactivate = function_text(source, get_function(tree, "on_deactivate"))

    forbidden_deactivate_text = (
        "_deregister_requested",
        "_request_deregister(",
        "DeregisterModule",
    )

    for forbidden in forbidden_deactivate_text:
        if forbidden in deactivate:
            validator.error(
                "deregister_on_deactivate",
                (
                    "Normal on_deactivate() must only stop health/output activity; "
                    f"it must not contain {forbidden}."
                ),
            )

    image_callback = function_text(source, get_function(tree, "image_cb"))

    if "_registered" not in image_callback:
        validator.error(
            "output_not_registration_gated",
            "image_cb() must prevent processing/output before registration.",
        )

    if "self._pub.publish" not in image_callback:
        validator.error(
            "missing_detection_publish",
            "Original YOLO detection publication is missing.",
        )

    deregister_response = function_text(
        source,
        get_function(tree, "_handle_deregister_response"),
    )

    if "rclpy.shutdown()" in deregister_response:
        validator.error(
            "shutdown_in_async_callback",
            (
                "Do not call rclpy.shutdown() directly inside the asynchronous "
                "deregistration response callback."
            ),
        )

    if "heartbeat_liveliness_ms" in source:
        if "QoSLivelinessPolicy.MANUAL_BY_TOPIC" not in source:
            validator.error(
                "missing_liveliness_qos",
                (
                    "heartbeat_liveliness_ms is declared but manual-by-topic "
                    "liveliness is not applied to the heartbeat publisher QoS."
                ),
            )

        if "liveliness_lease_duration" not in source:
            validator.error(
                "missing_liveliness_lease",
                (
                    "Heartbeat liveliness lease duration is not applied to "
                    "the actual publisher QoS."
                ),
            )

    if "self.output_topic" in source and (
        'create_lifecycle_publisher(\n'
        '            DetectionArray, self.output_topic'
    ) in source:
        validator.warning(
            "absolute_output_publisher",
            (
                "Prefer the original relative 'detections' publisher and verify "
                "that output_topic matches its resolved topic."
            ),
        )

    configure = get_function(tree, "on_configure")
    activate = get_function(tree, "on_activate")
    deactivate_node = get_function(tree, "on_deactivate")
    cleanup = get_function(tree, "on_cleanup")
    shutdown = get_function(tree, "on_shutdown")

    for name, node in (
        ("on_configure", configure),
        ("on_activate", activate),
        ("on_deactivate", deactivate_node),
        ("on_cleanup", cleanup),
        ("on_shutdown", shutdown),
    ):
        if node is None:
            validator.error(
                "missing_lifecycle_callback",
                f"Original lifecycle callback {name}() is missing.",
            )


def validate_launch(
    validator: Validator,
    generic_launch: Path,
    model_launch: Path,
) -> None:
    generic_text = generic_launch.read_text()
    model_text = model_launch.read_text()

    try:
        ast.parse(generic_text, filename=str(generic_launch))
        ast.parse(model_text, filename=str(model_launch))
    except SyntaxError as error:
        validator.error("launch_syntax", str(error))
        return

    if 'default_value="/image_raw"' in generic_text:
        validator.error(
            "generic_default_changed",
            (
                "Generic yolo.launch.py must preserve its original "
                "/camera/rgb/image_raw default."
            ),
        )

    if 'default_value="/camera/rgb/image_raw"' not in generic_text:
        validator.error(
            "missing_generic_default",
            "Original generic input topic default is missing.",
        )

    if "yolo_health.yaml" not in generic_text:
        validator.error(
            "health_yaml_not_loaded",
            "Generic launch file does not load yolo_health.yaml.",
        )

    if 'input_image_topic", default="/image_raw"' not in model_text:
        validator.warning(
            "model_input_override",
            (
                "yolov8.launch.py does not default the package-specific "
                "input topic to /image_raw."
            ),
        )


def validate_package(
    validator: Validator,
    package_xml: Path,
    setup_path: Path,
) -> None:
    package_text = package_xml.read_text()
    setup_text = setup_path.read_text()

    for dependency in (
        "drone_health_interfaces",
        "std_msgs",
        "std_srvs",
        "rclpy",
    ):
        if dependency not in package_text:
            validator.error(
                "missing_dependency",
                f"package.xml is missing dependency: {dependency}",
            )

    if "yolo_health.yaml" not in setup_text:
        validator.error(
            "yaml_not_installed",
            "setup.py does not install yolo_health.yaml.",
        )


def validate_yaml(
    validator: Validator,
    yaml_path: Path,
) -> None:
    parameters = load_yaml_parameters(validator, yaml_path)

    if not parameters:
        return

    required = (
        "module_name",
        "critical",
        "heartbeat_topic",
        "heartbeat_period_ms",
        "heartbeat_deadline_ms",
        "heartbeat_liveliness_ms",
        "output_topic",
        "output_deadline_ms",
        "request_deregister_service",
    )

    for name in required:
        if name not in parameters:
            validator.error(
                "missing_yaml_parameter",
                f"YAML is missing parameter: {name}",
            )

    heartbeat_period = parameters.get("heartbeat_period_ms")
    heartbeat_deadline = parameters.get("heartbeat_deadline_ms")

    if (
        isinstance(heartbeat_period, int)
        and isinstance(heartbeat_deadline, int)
        and heartbeat_deadline <= heartbeat_period
    ):
        validator.error(
            "unsafe_heartbeat_deadline",
            "heartbeat_deadline_ms must be greater than heartbeat_period_ms.",
        )

    if parameters.get("output_topic") != "/yolo/detections":
        validator.error(
            "wrong_output_topic",
            "YOLO owned output must be /yolo/detections.",
        )

    if parameters.get("heartbeat_topic") != "/yolo/heartbeat":
        validator.error(
            "wrong_heartbeat_topic",
            "YOLO heartbeat must be /yolo/heartbeat.",
        )

    if parameters.get("request_deregister_service") != "/yolo/request_deregister":
        validator.error(
            "wrong_deregister_service",
            "Deregister Trigger service must be /yolo/request_deregister.",
        )

    output_deadline = parameters.get("output_deadline_ms")

    if not isinstance(output_deadline, int) or output_deadline <= 0:
        validator.error(
            "invalid_output_deadline",
            "output_deadline_ms must be a positive integer.",
        )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate a generated YOLO Drone Health adaptation."
    )
    parser.add_argument(
        "adaptation_directory",
        type=Path,
        help="Root directory containing the generated adaptation.",
    )
    args = parser.parse_args()

    root = args.adaptation_directory.expanduser().resolve()
    validator = Validator()

    if not root.is_dir():
        print(f"Adaptation directory does not exist: {root}")
        return 2

    required_files = {
        "source": "yolo_ros/yolo_ros/yolo_node.py",
        "yaml": "yolo_ros/config/yolo_health.yaml",
        "setup": "yolo_ros/setup.py",
        "package": "yolo_ros/package.xml",
        "generic_launch": "yolo_bringup/launch/yolo.launch.py",
        "model_launch": "yolo_bringup/launch/yolov8.launch.py",
    }

    found: dict[str, Path] = {}

    for name, suffix in required_files.items():
        path = find_file(root, suffix)

        if path is None:
            validator.error(
                "missing_file",
                f"Missing or duplicated required file: {suffix}",
            )
        else:
            found[name] = path

    if validator.errors:
        return validator.report()

    validate_python(validator, found["source"])
    validate_yaml(validator, found["yaml"])
    validate_package(
        validator,
        found["package"],
        found["setup"],
    )
    validate_launch(
        validator,
        found["generic_launch"],
        found["model_launch"],
    )

    return validator.report()


if __name__ == "__main__":
    sys.exit(main())
