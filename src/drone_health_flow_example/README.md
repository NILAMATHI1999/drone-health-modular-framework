# drone_health_flow_example

Flow velocity example package for the drone health monitoring framework.

## Purpose

This package provides a simulated velocity/flow sensor node.

It is used to test SafetyFusion braking-distance logic without requiring real optical-flow or
odometry hardware.

## Nodes

- `simulated_flow_sensor_node`

## Data Flow

```text
simulated_flow_sensor_node
-> /vehicle/velocity
-> safety_fusion_node
```

## Build

```bash
colcon build --packages-select drone_health_flow_example
```

## Run

Default stationary/base behavior:

```bash
ros2 run drone_health_flow_example simulated_flow_sensor_node
```

Moving demo behavior:

```bash
ros2 run drone_health_flow_example simulated_flow_sensor_node --ros-args -p simulate_motion:=true
```

## Expected Behavior

The node publishes velocity and heartbeat for testing health monitoring and SafetyFusion.

## Failure Behavior

If this node stops, HealthMonitor should report flow failure and SafetyFusion should not use stale
velocity.

## Real Hardware Replacement

For real hardware, replace this simulated node with an optical-flow, visual-odometry, wheel-
odometry, or autopilot odometry source that publishes compatible velocity data on
`/vehicle/velocity`.
