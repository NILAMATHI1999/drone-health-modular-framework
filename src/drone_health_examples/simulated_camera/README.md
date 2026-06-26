# simulated_camera_node

## Purpose

Simulates an optional camera/inspection module.

It demonstrates node-side deregistration: the camera can notify ManagementNode that it is leaving the monitored system instead of simply disappearing as a failure.

## Inputs

- `/mission/phase` (`std_msgs/msg/String`)
- `/management/set_module_inactive` (`drone_health_interfaces/srv/SetModuleInactive`)

## Outputs

- `/camera/image_raw` (`sensor_msgs/msg/Image`)
- `/camera/heartbeat` (`std_msgs/msg/String`)
- `/camera/request_deregister` (`std_srvs/srv/Trigger`)

## Parameters

Typical parameters:

- `frame_id`
- `publish_period_ms`
- `image_deadline_ms`
- `heartbeat_deadline_ms`
- `heartbeat_liveliness_ms`
- `image_width`
- `image_height`

## Run command

```bash
ros2 run drone_health_examples simulated_camera_node
```

Request planned deregistration:

```bash
ros2 service call /camera/request_deregister std_srvs/srv/Trigger
```

## Expected Behavior

The node publishes camera images and heartbeat while active.

When it receives a mission phase such as `INSPECTION_COMPLETE`, it can request planned deregistration through ManagementNode.

For explicit demo testing, `/camera/request_deregister` can be called to request deregistration.

## Failure Behavior

If the camera shuts down without deregistering, HealthMonitor should report it as stale or failed.

If the camera deregisters through ManagementNode, HealthMonitor should show it as planned inactive/deregistered instead of failed.

## Real System Meaning

A real optional camera, inspection payload, or removable module may officially leave the system after its job is complete or before controlled shutdown.
