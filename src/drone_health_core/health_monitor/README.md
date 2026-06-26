# health_monitor_node

## Purpose

Monitors ROS topic/module health and publishes standardized health status.

HealthMonitor supports both configured monitoring from `health_monitor.yaml` and runtime heartbeat
monitoring from `/management/state`.

## Inputs

- configured topics from `health_monitor.yaml`
- `/management/state` (`drone_health_interfaces/msg/ManagementState`)

Configured topic message types currently supported:

- `string`
- `float32`
- `twist_stamped`
- `laser_scan`
- `image`

## Outputs

- `/health/status` (`drone_health_interfaces/msg/HealthStatus`)

## Parameters

Configured in `health_monitor.yaml`.

Monitor entries use fields such as:

- `node_name`
- `topic_name`
- `kind`
- `message_type`
- `reliability`
- `deadline_ms`
- `liveliness_ms`
- `timeout_ms`

## Run command

```bash
ros2 run drone_health_core health_monitor_node --ros-args --params-file /home/nila/Desktop/drone_health_modular_ws/src/drone_health_core/health_monitor/health_monitor.yaml
```

## Expected Behavior

HealthMonitor creates subscriptions from the YAML configuration and publishes health status for each
monitored topic.

It can detect:

- deadline missed
- liveliness lost
- QoS incompatibility
- fallback timeout when no recent message is received

Runtime-registered modules are published by ManagementNode through `registry_topic_*` fields.
Current runtime support dynamically monitors heartbeat topics.

## Planned Inactive Behavior

HealthMonitor listens to `/management/state`.

If ManagementNode marks a topic as planned inactive, HealthMonitor publishes:

```text
INACTIVE
```

with reasons such as:

- `MAINTENANCE`
- `DEREGISTERED`
- `OPTIONAL_DISABLED`
- `MISSION_NOT_REQUIRED`

This prevents planned shutdown from appearing as an unexpected failure.

## Failure Behavior

If a monitored topic stops unexpectedly, HealthMonitor reports stale, deadline missed, liveliness
lost, or timeout depending on the configured checks.

If a topic is planned inactive, HealthMonitor reports inactive instead of failure.

## Ctrl+C vs Deregister

```text
Ctrl+C / crash / power loss
-> no deregistration request
-> heartbeat stops
-> HealthMonitor reports stale/deadline/liveliness failure

planned deregister
-> ManagementNode marks topic inactive
-> HealthMonitor reports INACTIVE / DEREGISTERED
```

## Current Limitation

Runtime data topics are registered as metadata but are not generically monitored yet.

```text
YAML topics = full configured monitoring
runtime topics = heartbeat monitoring only
```

Future work can add generic serialized-message monitoring or additional supported message-type
handlers for runtime data topics.
