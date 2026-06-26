# drone_health_lidar_example

LiDAR example package for the drone health monitoring framework.

## Purpose

This package demonstrates a LiDAR pipeline using a simulated LiDAR driver and an obstacle processor.

The driver publishes raw scan data. The processor converts raw scan data into the nearest obstacle
distance used by SafetyFusion.

## Nodes

- `simulated_lidar_driver_node`
- `lidar_obstacle_processor_node`

## Data Flow

```text
simulated_lidar_driver_node
-> /lidar/scan
-> lidar_obstacle_processor_node
-> /lidar/nearest_obstacle
-> safety_fusion_node
```

## Build

```bash
colcon build --packages-select drone_health_lidar_example
```

## Run

```bash
ros2 run drone_health_lidar_example simulated_lidar_driver_node
```

```bash
ros2 run drone_health_lidar_example lidar_obstacle_processor_node
```

## Expected Behavior

The simulated driver publishes LiDAR scan data. The obstacle processor publishes nearest obstacle
distance.

## Failure Behavior

If the simulated driver or obstacle processor stops, HealthMonitor should report stale/failure and
SafetyFusion should not trust stale obstacle data.

## Real Hardware Replacement

For real hardware, replace `simulated_lidar_driver_node` with the real LiDAR driver that publishes
`sensor_msgs/msg/LaserScan` on `/lidar/scan`.

The obstacle processor can stay the same if the real driver publishes compatible scan data.
