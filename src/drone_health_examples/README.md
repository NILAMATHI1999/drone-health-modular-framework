# drone_health_examples

Optional demo nodes for the drone health monitoring framework.

## Purpose

This package contains example nodes used to demonstrate optional modules, mission phase behavior,
and node-side deregistration.

These nodes are not required for the core health/supervisor system.

## Nodes

- `simulated_camera_node`
- `mission_manager_node`

## Build

```bash
colcon build --packages-select drone_health_examples
```

## Run

```bash
ros2 run drone_health_examples simulated_camera_node
```

```bash
ros2 run drone_health_examples mission_manager_node
```

## Expected Behavior

These nodes support demos, experiments, and teaching examples.

The camera demonstrates optional module behavior and planned deregistration. The mission manager
demonstrates simple mission phase messages.

## Failure Behavior

If these example nodes are not running, the core system can still run.

Core reusable logic remains in:

- `drone_health_core`
- `drone_health_interfaces`
