# lidar_obstacle_processor_node

## Purpose

Consumes raw LiDAR scan data and publishes the nearest valid obstacle distance.

## Inputs

- `/lidar/scan` (`sensor_msgs/msg/LaserScan`)

## Outputs

- `/lidar/nearest_obstacle` (`std_msgs/msg/Float32`)
- `/lidar_obstacle_processor/heartbeat` (`std_msgs/msg/String`)

## Parameters

Configured in node parameters if needed.

Common processing settings may include:

- minimum valid range
- maximum valid range
- publish period
- heartbeat timing

## Run command

```bash
ros2 run drone_health_lidar_example lidar_obstacle_processor_node
```

## Expected Behavior

Publishes the nearest valid obstacle distance from the LiDAR scan.

SafetyFusion uses this value with vehicle velocity to decide whether the drone has enough stopping clearance.

## Failure Behavior

If scan input is missing or invalid, the processor should stop publishing valid obstacle distance or report invalid data depending on implementation.

HealthMonitor and SafetyFusion should prevent stale obstacle data from being used for safety decisions.
