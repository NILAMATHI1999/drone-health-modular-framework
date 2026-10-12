# Tools

Standalone scripts for checking node adaptations and measuring HealthMonitor behaviour.

| Script | Needs ROS 2? | Purpose |
|---|---|---|
| `validate_adaptation.py` | No | Check any adapted node against an adaptation manifest |
| `validate_yolo_adaptation.py` | No | Fixed checks for the YOLO (`yolo_ros`) adaptation case study |
| `adaptation_manifest.example.yaml` | — | Example manifest (the YOLO case study) |
| `measure_health_detection.py` | Yes | Time how long HealthMonitor takes to report a stopped topic as `STALE` |

The validators only need Python 3.10+ and PyYAML (`pip install pyyaml`). They read files; they do not build, run or modify anything.

Run all commands from the repository root.

---

## `validate_adaptation.py` — generic adaptation validator

Checks an adapted ROS 2 package (C++ or Python, lifecycle or not) against a manifest that describes what the node should register, publish and contain. Use it after adapting a node by hand or with `project_agent`.

```bash
python3 tools/validate_adaptation.py <adaptation_dir> --manifest <manifest.yaml>

# Example
python3 tools/validate_adaptation.py ~/yolo_ws/src \
  --manifest tools/adaptation_manifest.example.yaml
```

- `<adaptation_dir>` — root folder that the paths in `files:` are relative to
- `--manifest` — the adaptation manifest YAML (required)

**Exit codes:** `0` = PASS (warnings allowed), `1` = FAIL (at least one error), `2` = directory or manifest could not be read. This makes it usable in CI.

### Example output

```
Generic Drone Health adaptation validation

Profile: python-lifecycle-processing
Root: /home/user/yolo_ws/src

[ERROR] missing_owned_output: Owned output is missing from source: /yolo/detections

Validation: FAIL (1 errors)
```

If any file listed in the manifest is missing, the validator stops after the file check and reports only those errors.

### Manifest format

```yaml
profile: "python-lifecycle-processing"   # free-text label shown in the report
ros_distribution: "jazzy"

node:
  name: "yolo_node"
  namespace: "/yolo"
  module_name: "yolo_node"               # name sent to /management/register_module
  language: "python"                     # cpp | python
  lifecycle: true

topics:
  external_inputs:                       # topics the node subscribes to but does NOT own
    - name: "/image_raw"
      message_type: "sensor_msgs/msg/Image"
  owned_outputs:                         # topics the node publishes and registers
    - name: "/yolo/detections"
      message_type: "yolo_msgs/msg/DetectionArray"
      kind: "data"
      deadline_ms: 1000
  heartbeat:
    name: "/yolo/heartbeat"
    message_type: "std_msgs/msg/String"
    period_ms: 200
    deadline_ms: 500                     # must be greater than period_ms
    liveliness_ms: 0

services:
  request_deregister: "/yolo/request_deregister"
  register: "/management/register_module"      # must be exactly this
  deregister: "/management/deregister_module"  # must be exactly this

files:                                   # paths relative to <adaptation_dir>
  source:       ["yolo_ros/yolo_ros/yolo_node.py"]
  yaml:         ["yolo_ros/config/yolo_health.yaml"]
  build:        ["yolo_ros/setup.py"]          # setup.py or CMakeLists.txt
  dependencies: ["yolo_ros/package.xml"]
  launch:       ["yolo_bringup/launch/yolo.launch.py"]
```

The full schema rules are in [`src/drone_health_registrable_template/README.md`](../src/drone_health_registrable_template/README.md#required-adaptation-manifest-format).

### What it checks

| Area | Checks |
|---|---|
| **Manifest** | Required sections (`profile`, `ros_distribution`, `node`, `topics`, `services`, `files`); node fields; `language` is `cpp` or `python`; heartbeat fields present and `deadline_ms > period_ms`; register/deregister services point to `/management/...` |
| **Files** | Every file listed under `files:` exists |
| **Source integration** | Uses `RegisterModule`, `DeregisterModule`, `MonitorSpec`, the correct Management services, a `Trigger` service for planned deregistration, and a heartbeat; no register/deregister services outside `/management` |
| **Topic ownership** | Every owned output and the heartbeat appear in the source with their message types; external inputs are **not** registered as monitors; no topic is both external and owned |
| **MonitorSpec usage** | Flags invented fields such as `monitor_spec.name` or `monitor_spec.type` |
| **Python specifics** | Valid syntax; no `Duration(milliseconds=...)` (not supported by Jazzy `rclpy`); `MANUAL_BY_TOPIC` liveliness QoS and lease applied when liveliness is configured; `assert_liveliness` used safely |
| **Lifecycle nodes** | All lifecycle callbacks present (`on_configure`, `on_activate`, `on_deactivate`, `on_cleanup`, `on_shutdown`); a normal `on_deactivate()` must not deregister; no `rclpy.shutdown()` inside async callbacks |
| **ROS YAML** | Valid `ros__parameters`; parameters match the manifest; heartbeat deadline larger than its period; output topic parameters and deadlines present |
| **Dependencies** | `package.xml` is valid and depends on `drone_health_interfaces`, `std_msgs`, `std_srvs` |
| **Build files** | YAML config is installed; required CMake dependencies; install target present (warning if not confirmed) |
| **Launch files** | Valid Python syntax; at least one launch file loads the ROS YAML |
| **Cross-file contract** | Output message types agree between manifest and source |

---

## `validate_yolo_adaptation.py` — YOLO case study

A fixed-layout validator for adapting the [`yolo_ros`](https://github.com/mgonzs13/yolo_ros) detection node. It does not use a manifest; the expected files and values are built in.

```bash
python3 tools/validate_yolo_adaptation.py <adaptation_dir>
```

It expects each of these files exactly once anywhere under `<adaptation_dir>`:

```
yolo_ros/yolo_ros/yolo_node.py
yolo_ros/config/yolo_health.yaml
yolo_ros/setup.py
yolo_ros/package.xml
yolo_bringup/launch/yolo.launch.py
yolo_bringup/launch/yolov8.launch.py
```

On top of the generic rules (lifecycle callbacks, liveliness QoS, no registered input topic, no deregistration on normal deactivate, dependencies, installed YAML), it checks YOLO-specific details:

- detections are published and the output publisher is gated on registration
- the output publisher uses a relative topic name
- the health YAML contains `module_name`, `critical`, `heartbeat_topic`, `heartbeat_period_ms`, `heartbeat_deadline_ms`, `heartbeat_liveliness_ms`, `output_topic`, `output_deadline_ms`, `request_deregister_service`
- heartbeat, output and deregister topics have the expected names, and deadlines are valid
- the launch files load the health YAML without overriding the model input or changing the generic defaults

Exit codes are the same as `validate_adaptation.py` (`0` pass, `1` fail, `2` bad directory).

Use `validate_adaptation.py` with `adaptation_manifest.example.yaml` for the same case when you want the generic checks; use this script when you want the stricter YOLO-specific ones.

---

## `measure_health_detection.py` — stale-detection latency

Subscribes to `/health/status` and prints how long it takes for HealthMonitor to report a topic as `STALE` after its publisher stops. Use it to compare QoS-event detection on DDS with the timeout fallback on Zenoh.

```bash
source /opt/ros/jazzy/setup.bash
source install/setup.bash
# For Zenoh: export RMW_IMPLEMENTATION=rmw_zenoh_cpp

python3 tools/measure_health_detection.py [topic]   # default: /camera/heartbeat
```

Steps:

1. Start the core nodes (Management, HealthMonitor) and `simulated_camera_node`.
2. Start the script. It prints `Waiting for STALE status on /camera/heartbeat. Kill the camera node now.`
3. Stop the camera node right away (Ctrl+C in its terminal).

Output:

```
Detection result
  topic: /camera/heartbeat
  status: STALE
  reason: ...
  message: message timeout fallback
  last_update_age_s: 0.612
  wall_time_to_detect_s: 0.734
```

- `last_update_age_s` — age of the last message when HealthMonitor declared it stale
- `wall_time_to_detect_s` — time from starting the script to the `STALE` report. The clock starts when the script starts, not when you kill the node, so stop the node immediately to get a meaningful number.
- On Zenoh, `message` is `message timeout fallback`; on DDS it reflects the QoS event that fired.

The script only reacts to status messages whose `node_name` is `camera`. To measure another module, change the `node_name` check in `handle_health_status()`.
