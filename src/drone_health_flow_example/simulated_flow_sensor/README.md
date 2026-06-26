# simulated_flow_sensor_node

## Purpose

Publishes simulated vehicle velocity and a heartbeat topic.

## Inputs

None.

## Outputs

- `/vehicle/velocity` (`geometry_msgs/msg/TwistStamped`)
- `/flow/heartbeat` (`std_msgs/msg/String`)

## Parameters

- `frame_id`
- `publish_period_ms`
- `velocity_deadline_ms`
- `heartbeat_deadline_ms`
- `heartbeat_liveliness_ms`
- `simulate_motion`
- `stationary_forward_velocity_mps`
- `base_forward_velocity_mps`
- `side_drift_amplitude_mps`

## Run command

```bash
ros2 run drone_health_flow_example simulated_flow_sensor_node
```

Moving demo behavior:

```bash
ros2 run drone_health_flow_example simulated_flow_sensor_node --ros-args -p simulate_motion:=true
```

## Expected Behavior

By default, the node publishes stationary/base velocity:

```text
linear.x = 0.0
linear.y = 0.0
linear.z = 0.0
```

When started with `simulate_motion:=true`, it publishes changing forward velocity and side drift for
safety testing.

## Failure Behavior

If this node stops or its velocity/heartbeat topic becomes stale, HealthMonitor should report flow
sensor failure.

SafetyFusion should not use stale velocity for braking-distance calculation.
