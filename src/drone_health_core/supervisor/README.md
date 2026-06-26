# supervisor_node

## Purpose

Combines safety, health, and management state into the final system decision.

The supervisor is the final decision layer. It does not calculate obstacle braking distance and it
does not monitor raw sensor QoS directly. It uses the outputs from SafetyFusion, HealthMonitor, and
ManagementNode to decide whether the system is normal, holding, in failsafe, or emergency stopped.

## Inputs

- `/safety/status` (`drone_health_interfaces/msg/SafetyStatus`)
- `/health/status` (`drone_health_interfaces/msg/HealthStatus`)
- `/management/state` (`drone_health_interfaces/msg/ManagementState`)

## Outputs

- `/supervisor/status` (`drone_health_interfaces/msg/SupervisorStatus`)
- `/supervisor/heartbeat` (`std_msgs/msg/String`)

## Parameters

Configured in `supervisor.yaml`.

Important parameters:

- `safety_status_topic`
- `health_status_topic`
- `management_state_topic`
- `supervisor_status_topic`
- `heartbeat_topic`
- `required_health_topics`
- `safety_status_timeout_ms`
- `health_status_timeout_ms`
- `management_state_timeout_ms`
- `heartbeat_period_ms`
- `heartbeat_deadline_ms`
- `heartbeat_liveliness_ms`

## Run command

```bash
ros2 run drone_health_core supervisor_node --ros-args --params-file /home/nila/Desktop/drone_health_modular_ws/src/drone_health_core/supervisor/supervisor.yaml
```

## Expected Behavior

When the system is healthy and SafetyFusion reports safe, the supervisor publishes a normal state
and allows commands.

Example normal behavior:

```text
SafetyFusion SAFE
required health topics OK
management state fresh
no critical planned inactive module
-> Supervisor NORMAL / command allowed
```

If SafetyFusion reports unsafe because an obstacle is too close, the supervisor publishes an
emergency or blocked decision depending on the configured logic.

If required health topics fail, become stale, or are not received, the supervisor blocks commands
and reports a failsafe/hold reason.

If ManagementNode marks a non-critical module planned inactive, the supervisor should not treat that
as an unexpected failure.

## Failure Behavior

If `/safety/status` becomes stale or is missing, the supervisor should block commands because the
latest safety decision is not trustworthy.

If `/health/status` becomes stale or required health topics fail, the supervisor should block
commands or enter failsafe.

If `/management/state` becomes stale, the supervisor should avoid trusting planned inactive/runtime
registry state and should fail safe or hold according to the supervisor logic.

If a critical module is planned inactive during an active mission, the supervisor should prevent
normal operation.

## Responsibility Boundary

The supervisor does not:

- read raw LiDAR scans
- calculate braking distance
- publish sensor health directly
- manage runtime registration
- run mission sequencing
- control PX4/ArduPilot directly
- contain dashboard logic

Those responsibilities belong to other packages:

```text
HealthMonitor = health detection
SafetyFusion = safety calculation
ManagementNode = mission/maintenance/registration state
Supervisor = final system decision
Dashboard = visualization and operator commands
```
