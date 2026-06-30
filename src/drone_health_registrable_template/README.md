# drone_health_registrable_template

Reusable template package for ROS 2 modules that need runtime registration and planned
deregistration with the drone health monitoring framework.

## Purpose

This package shows future students how a module can officially join and leave the monitored system
while ROS is running.

It demonstrates the correct pattern for:

- heartbeat publishing
- runtime registration
- planned deregistration
- approved shutdown
- stale/failure behavior when a node disappears without deregistering

## Package Contents

```text
template_node/
  registrable_template_node.cpp
  README.md
```

## Architecture Role

This package is an example/template package. It does not contain core management logic.

Correct responsibility split:

```text
registrable node -> calls register/deregister services
ManagementNode   -> approves/rejects registration and deregistration
HealthMonitor    -> monitors heartbeat and reports OK/STALE/INACTIVE
Dashboard         -> visualizes the result
```

## Build

```bash
colcon build --packages-select drone_health_registrable_template
```

## Run

Start core nodes first:

```text
ManagementNode
HealthMonitor
```

Then run:

```bash
ros2 run drone_health_registrable_template registrable_template_node
```

## Expected Behavior

The template node registers itself at startup, publishes a heartbeat, and appears in ManagementNode
registry state.

When `/template/request_deregister` is called, it requests approved deregistration before shutting
down.

## Failure Behavior

If the node is killed with Ctrl+C or crashes, ManagementNode does not mark it planned inactive.
HealthMonitor reports stale or deadline failure.

If the node uses planned deregistration, HealthMonitor reports `INACTIVE / DEREGISTERED`.

## Current Scope

Currently, this template supports runtime heartbeat monitoring.

Data topics are sent as registry metadata, but generic dynamic data-topic monitoring is future work.

## Future Extension

Future students can extend this template for:

- camera modules
- GPS modules
- network modules
- inspection payloads
- removable robot components
