# drone_health_core

Core health, management, and supervision package for the drone health monitoring framework.

## Purpose

This package contains the reusable backend nodes that future projects can use for health monitoring,
system management, and supervisor decisions.

## Nodes

```text
health_monitor/
  health_monitor_node.cpp
  health_monitor.yaml

management/
  management_node.cpp
  management.yaml

supervisor/
  supervisor_node.cpp
  supervisor.yaml
```

## Responsibility Split

HealthMonitor:

- monitors YAML-configured topics
- dynamically monitors runtime-registered heartbeat topics
- handles QoS deadline, liveliness, incompatibility, and timeout failures
- marks planned inactive topics as INACTIVE instead of FAILED

ManagementNode:

- handles mission_active
- handles maintenance mode
- handles planned inactive modules
- handles runtime registration
- handles planned deregistration

Supervisor:

- combines health, safety, and management state into the final system decision

## What This Package Does Not Do

This package does not contain:

- mission sequencing
- waypoint navigation
- return-home implementation
- PX4/ArduPilot flight control logic
- dashboard frontend
- simulated sensors

Those belong to separate modules or future robot/autonomy integration.

## Build

```bash
colcon build --packages-select drone_health_core
```

## Run

```bash
ros2 run drone_health_core management_node --ros-args --params-file install/drone_health_core/share/drone_health_core/management/management.yaml
```

```bash
ros2 run drone_health_core health_monitor_node --ros-args --params-file install/drone_health_core/share/drone_health_core/health_monitor/health_monitor.yaml
```

```bash
ros2 run drone_health_core supervisor_node --ros-args --params-file install/drone_health_core/share/drone_health_core/supervisor/supervisor.yaml
```

## Runtime Registration / Deregistration

Runtime registration allows optional modules to join while ROS is already running.

Runtime deregistration allows modules to officially leave without being treated as unexpected
failures.

Runtime registration currently adds dynamic heartbeat monitoring.

Data topics are stored as registry metadata for dashboard/future extension, but generic runtime
data-topic monitoring is future work.

Flow:

```text
registrable node
-> ManagementNode register/deregister service
-> ManagementNode publishes /management/state
-> HealthMonitor dynamically monitors heartbeat
-> Supervisor/dashboard see correct state
```

## Mission Active

`mission_active` is a simple high-level state:

```text
false = base / idle / preflight
true  = active mission / running task
```

It is intentionally simple. Complex mission behavior should be handled by future mission systems,
behavior trees, state machines, or PX4/ArduPilot integration.
