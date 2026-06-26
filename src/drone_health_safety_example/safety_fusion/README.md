# safety_fusion_node

## Purpose

Calculates whether the drone has enough clearance to stop safely using obstacle distance, velocity, reaction time, deceleration, and safety margin.

## Inputs

- `/lidar/nearest_obstacle` (`std_msgs/msg/Float32`)
- `/vehicle/velocity` (`geometry_msgs/msg/TwistStamped`)
- `/health/status` (`drone_health_interfaces/msg/HealthStatus`)

## Outputs

- `/safety/status` (`drone_health_interfaces/msg/SafetyStatus`)
- `/safety_fusion/heartbeat` (`std_msgs/msg/String`)

## Parameters

Configured in `safety_fusion.yaml`.

Important parameters:

- `max_deceleration_mps2`
- `reaction_time_s`
- `safety_margin_m`
- `required_health_topics`
- `health_status_timeout_ms`
- `obstacle_timeout_ms`
- `velocity_timeout_ms`

## Run command

```bash
ros2 run drone_health_safety_example safety_fusion_node --ros-args --params-file /home/nila/Desktop/drone_health_modular_ws/src/drone_health_safety_example/safety_fusion/safety_fusion.yaml
```

## Expected Behavior

When obstacle and velocity data are fresh and required health topics are OK:

- publishes `SAFE` if clearance is enough
- publishes `UNSAFE` if obstacle is too close for braking distance

## Failure Behavior

If required sensor health fails or input data becomes stale:

- does not calculate braking distance using stale data
- publishes a blocked/unsafe status
- supervisor should use this to hold or fail safe the system

Supervisor decides whether this becomes HOLD or FAILSAFE based on mission state and system context.
