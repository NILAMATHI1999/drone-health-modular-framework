from app.output_validator import (
    validate_adaptation,
)


VALID_ANSWER = """
```cpp
create_client<RegisterModule>("/management/register_module");
create_client<DeregisterModule>("/management/deregister_module");
heartbeat_topic_ = "/battery/heartbeat";
monitor.topic_name = data_topic_;
monitor.kind = "data";
monitor.message_type = "std_msgs/msg/Float32";
request->monitors.push_back(monitor);
request_deregister_service_ = create_service<Trigger>(
  request_deregister_service_name_, callback);
```

```yaml
battery_publisher:
  ros__parameters:
    publish_period_ms: 1000
    heartbeat_deadline_ms: 1500
    data_deadline_ms: 2000
    request_deregister_service: "/battery/request_deregister"
```

```cmake
ament_target_dependencies(target rclcpp drone_health_interfaces)
```

```xml
<package>
<depend>rclcpp</depend>
<depend>drone_health_interfaces</depend>
</package>
```

```yaml
profile: "publisher"
ros_distribution: "jazzy"
node:
  name: "battery_publisher"
  module_name: "battery_publisher"
  language: "cpp"
  lifecycle: false
topics:
  external_inputs: []
  owned_outputs:
    - name: "/battery/voltage"
      message_type: "std_msgs/msg/Float32"
      kind: "data"
      deadline_ms: 2000
  heartbeat:
    name: "/battery/heartbeat"
    message_type: "std_msgs/msg/String"
    period_ms: 1000
    deadline_ms: 1500
    liveliness_ms: 0
services:
  register: "/management/register_module"
  deregister: "/management/deregister_module"
  request_deregister: "/battery/request_deregister"
files:
  source:
    - "src/battery_publisher.cpp"
  yaml:
    - "config/battery_publisher.yaml"
  build:
    - "CMakeLists.txt"
  dependencies:
    - "package.xml"
  launch: []
```
"""


INVALID_ANSWER = """
```cpp
monitor_spec_.name = "/battery/voltage";
monitor_spec_.type = "std_msgs/msg/Float32";
request->monitor_spec = monitor_spec_;
create_client<RegisterModule>("/register_module");
```

```yaml
battery_publisher:
  module_name: battery_publisher
  publish_period_ms: 1000
  data_deadline_ms: 1000
```
"""


def test_valid_adaptation_passes() -> None:
    report = validate_adaptation(VALID_ANSWER)

    assert report.passed


def test_invalid_qwen_patterns_fail() -> None:
    report = validate_adaptation(INVALID_ANSWER)
    issue_codes = {
        issue.code
        for issue in report.issues
    }

    assert not report.passed
    assert "invented_monitor_name" in issue_codes
    assert "invented_monitor_type" in issue_codes
    assert "singular_monitor_request" in issue_codes
    assert "missing_ros_parameters" in issue_codes



def test_missing_manifest_fails() -> None:
    answer = """
```cpp
create_client<RegisterModule>("/management/register_module");
create_client<DeregisterModule>("/management/deregister_module");
heartbeat
MonitorSpec monitor;
monitor.topic_name = "/battery/voltage";
monitor.kind = "data";
monitor.message_type = "std_msgs/msg/Float32";
request->monitors.push_back(monitor);
request_deregister
```

```yaml
battery_publisher:
  ros__parameters:
    publish_period_ms: 1000
    heartbeat_deadline_ms: 1500
    request_deregister_service: "/battery/request_deregister"
```

```cmake
ament_target_dependencies(target rclcpp drone_health_interfaces)
```

```xml
<package><depend>drone_health_interfaces</depend></package>
```
"""

    report = validate_adaptation(answer)
    issue_codes = {
        issue.code
        for issue in report.issues
    }

    assert not report.passed
    assert "missing_adaptation_manifest" in issue_codes


def test_manifest_old_schema_fails() -> None:
    answer = VALID_ANSWER.replace(
        'profile: "publisher"',
        'validator:\n  profile: "publisher"',
    ).replace(
        'name: "/battery/voltage"',
        'topic: "/battery/voltage"',
    )

    report = validate_adaptation(answer)
    issue_codes = {
        issue.code
        for issue in report.issues
    }

    assert not report.passed
    assert "missing_adaptation_manifest" in issue_codes


def test_snippet_file_names_fail() -> None:
    answer = VALID_ANSWER + """
CMakeLists_additions.txt
package_xml_dependencies.txt
"""

    report = validate_adaptation(answer)
    issue_codes = {
        issue.code
        for issue in report.issues
    }

    assert not report.passed
    assert "cmake_snippet_file" in issue_codes
    assert "package_snippet_file" in issue_codes
