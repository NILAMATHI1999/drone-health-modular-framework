# simulated_lidar_driver_node

## Purpose

Publishes simulated 2D LiDAR scan data and a heartbeat topic.

## Inputs

None.

## Outputs

- `/lidar/scan` (`sensor_msgs/msg/LaserScan`)
- `/lidar/heartbeat` (`std_msgs/msg/String`)

## Parameters

Configured in node parameters if needed.

Common simulation settings may include:

- scan range
- obstacle distance
- publish period
- heartbeat timing

## Run command

```bash
ros2 run drone_health_lidar_example simulated_lidar_driver_node
```

## Expected Behavior

Publishes periodic LiDAR scan data for testing health monitoring, obstacle processing, safety fusion, and supervisor behavior.

## Failure Behavior

If this node stops or its heartbeat/scan topic becomes stale, HealthMonitor should report LiDAR failure.

Supervisor should block or fail safe because LiDAR is a critical safety sensor.
