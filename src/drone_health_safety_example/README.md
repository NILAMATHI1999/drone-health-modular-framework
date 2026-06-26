# drone_health_safety_example

Example safety fusion package for the drone health monitoring framework.

## Purpose

This package contains the safety fusion node used to combine obstacle distance, vehicle velocity,
braking distance, and health status into one safety decision.

It is separated from the core package because safety logic can be replaced or customized for
different robots, drones, missions, or safety policies.

## Nodes

- `safety_fusion_node`

## Depends On

- `drone_health_interfaces`
- `rclcpp`
- `std_msgs`
- `geometry_msgs`

## Build

```bash
colcon build --packages-select drone_health_safety_example
```

## Run

```bash
ros2 run drone_health_safety_example safety_fusion_node --ros-args --params-file /home/nila/Desktop/drone_health_modular_ws/src/drone_health_safety_example/safety_fusion/safety_fusion.yaml
```

## Expected Behavior

SafetyFusion publishes `SAFE` when obstacle distance, velocity, and required health are valid and
enough stopping clearance exists.

It publishes unsafe/blocked status when braking clearance is insufficient or required health/input
freshness is bad.

## Failure Behavior

If required sensor health fails or input data becomes stale, SafetyFusion should not calculate
braking distance using stale data.
