# Template Node Source Files

This folder contains three reusable ROS 2 C++ template nodes for integrating future modules with the Drone Health Monitoring Framework.

## Files

| File | Executable | Pattern |
|---|---|---|
| `registrable_publisher_template_node.cpp` | `registrable_publisher_template_node` | Publisher + heartbeat + dynamic registration |
| `registrable_subscriber_template_node.cpp` | `registrable_subscriber_template_node` | Subscriber + processed output publisher + heartbeat + dynamic registration |
| `registrable_service_template_node.cpp` | `registrable_service_template_node` | Service provider + heartbeat + dynamic registration |

## Common Pattern

All templates use the same framework integration pattern:

```text
startup
-> create publishers/subscribers/services
-> call /management/register_module
-> send MonitorSpec[] for owned monitored topics
-> publish heartbeat while active
-> call /management/deregister_module before planned shutdown
```

## MonitorSpec Rule

Register only topics owned by the node.

| Node owns topic? | Register it? |
|---|---|
| Heartbeat topic published by this node | Yes |
| Data/output topic published by this node | Yes |
| Input topic subscribed from another node | No |
| Service name provided by this node | No, HealthMonitor monitors topics, not services |

## Publisher Template

Use for sensor or data-producing nodes.

Examples:

```text
camera driver
GPS publisher
battery monitor
LiDAR driver
network monitor
```

Registers:

```text
heartbeat topic
optional data topic
```

Future user replaces:

```text
publish_data()
message type
topic names
QoS timing
```

## Subscriber Template

Use for processing nodes.

Examples:

```text
YOLO detector
obstacle processor
localization filter
safety fusion input processor
```

Pattern:

```text
subscribe input topic
process latest input
publish output topic
publish heartbeat
register heartbeat + output topic
```

The input topic is not registered by this node because it belongs to the publisher that produces it.

## Service Template

Use for command/request-response nodes.

Examples:

```text
reset sensor
calibrate camera
take picture
save map
change mode
```

Pattern:

```text
provide service
publish heartbeat
register heartbeat
```

The service itself is not registered as a MonitorSpec because HealthMonitor monitors topic freshness, not service availability.

## Manual Planned Deregistration

Use the request deregister services for planned shutdown:

```bash
ros2 service call /template_publisher/request_deregister std_srvs/srv/Trigger "{}"
ros2 service call /template_subscriber/request_deregister std_srvs/srv/Trigger "{}"
ros2 service call /template_service/request_deregister std_srvs/srv/Trigger "{}"
```

A forced kill or Ctrl+C without deregistration is treated as unexpected failure and HealthMonitor may report stale/deadline/liveliness failure.

## Optional Auto Self-Deregistration

Each template has an optional parameter:

```text
auto_deregister_after_cycles
```

Default:

```text
0
```

Meaning:

```text
disabled
```

When set above zero, the node requests deregistration internally after that many active timer cycles.

This is only a generic example. Future users should replace the cycle-count condition with real task logic:

```text
task complete
calibration complete
mission phase finished
payload no longer required
optional sensor disabled
diagnostic task finished
```

## AI-Assisted Integration

These templates are intended to help future students or AI tools adapt existing ROS packages.

Use:

```text
publisher node -> publisher template pattern
processing node -> subscriber template pattern
service node -> service template pattern
mixed real node -> combine the relevant patterns
```

Keep unchanged:

```text
RegisterModule client
MonitorSpec[] creation
heartbeat publisher
DeregisterModule client
planned deregistration flow
```

## AI Usage Note

For AI-assisted integration, use these files as style references. The AI should copy the registration, heartbeat, MonitorSpec, and deregistration pattern, but it should not rewrite the whole node unnecessarily.

Tell the AI to keep the original node logic compact and only add the drone-health integration blocks.
