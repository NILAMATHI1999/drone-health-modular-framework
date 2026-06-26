# drone_health_interfaces

ROS 2 interface package for the drone health monitoring framework.

## Purpose

This package contains shared messages and services used by the health monitor, safety fusion,
supervisor, management node, dashboard bridge, examples, and registrable template nodes.

## Messages

- `msg/HealthStatus.msg`
- `msg/SafetyStatus.msg`
- `msg/SupervisorStatus.msg`
- `msg/ManagementState.msg`

## Services

- `srv/SetModuleInactive.srv`
- `srv/RegisterModule.srv`
- `srv/DeregisterModule.srv`

## Runtime Registry Fields

`ManagementState.msg` includes runtime registry fields used by ManagementNode, HealthMonitor, and
the dashboard.

Meaning:

```text
registry_* = known module/topic metadata in the current runtime
active_* = currently active / expected online
planned_inactive_* = intentionally inactive
```

Important fields:

```text
registry_modules
active_modules

planned_inactive_modules
planned_inactive_module_reasons
planned_inactive_topics
planned_inactive_topic_reasons

registry_topic_modules
registry_topics
registry_topic_types
registry_topic_critical
registry_topic_is_heartbeat
registry_topic_deadline_ms
registry_topic_liveliness_ms
```

Runtime heartbeat monitoring uses:

```text
registry_topic_is_heartbeat = true
registry_topic_types = std_msgs/msg/String
```

Data topics may be listed in the registry, but generic runtime data-topic monitoring is future work.

## Build

Place this package inside a ROS 2 workspace `src/` folder:

```bash
colcon build --packages-select drone_health_interfaces
```

## Used By

- `drone_health_core`
- `drone_health_safety_example`
- `drone_health_dashboard`
- `drone_health_examples`
- `drone_health_registrable_template`
