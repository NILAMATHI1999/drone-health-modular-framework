# registrable_template_node

## Purpose

Reusable ROS 2 template node for modules that need runtime registration and planned deregistration
with the drone health monitoring framework.

Future students can copy this node pattern when adding modules such as camera, GPS,
network, payload, or inspection modules.

## Inputs

- `/management/register_module` (`drone_health_interfaces/srv/RegisterModule`)
- `/management/deregister_module` (`drone_health_interfaces/srv/DeregisterModule`)
- `/template/request_deregister` (`std_srvs/srv/Trigger`)

## Outputs

- `/template/heartbeat` (`std_msgs/msg/String`)
- `/template/value` (`std_msgs/msg/Float32`, optional example data topic)

## Parameters

Typical parameters:

- `module_name`
- `critical`
- `heartbeat_topic`
- `publish_period_ms`
- `heartbeat_deadline_ms`
- `heartbeat_liveliness_ms`
- `publish_data_topic`
- `data_topic`
- `data_deadline_ms`

## Run command

```bash
ros2 run drone_health_registrable_template registrable_template_node
```

Request planned deregistration:

```bash
ros2 service call /template/request_deregister std_srvs/srv/Trigger "{}"
```

## Runtime Registration Flow

```text
node starts
-> calls /management/register_module
-> sends module name, critical flag, and full MonitorSpec list for heartbeat plus optional data topic
-> ManagementNode adds the module to the runtime registry
-> HealthMonitor dynamically monitors the registered heartbeat/data topics
-> dashboard can show the module as active
```

## Runtime Deregistration Flow

```text
operator or node requests planned shutdown
-> node calls /management/deregister_module
-> ManagementNode approves or rejects
-> if approved, node stops publishing and shuts down
-> HealthMonitor removes runtime subscriptions; dashboard removes the module health tiles
```

## Expected Behavior

After startup:

```text
/template/heartbeat -> OK
template_node appears in managed_modules
```

After planned deregistration:

```text
template_node appears in planned_inactive_modules
/template/heartbeat and /template/value health tiles are removed from the dashboard
```

## Failure Behavior

Ctrl+C, crash, or power loss does not call deregistration. ManagementNode state does not change and
HealthMonitor reports stale/deadline/liveliness failure.

Planned deregistration asks ManagementNode first and produces planned inactive state instead of
failure.

## QoS Rule

The heartbeat publisher QoS must match what the node registers.

Example:

```text
registered heartbeat_deadline_ms = 500
publisher must also use deadline 500 ms
```

If the node registers QoS that does not match its actual publisher QoS, HealthMonitor may report
`QOS_INCOMPATIBLE`, `DEADLINE_MISSED`, or `LIVELINESS_LOST`.

## Runtime Data Monitoring

This template dynamically registers and publishes both a heartbeat topic and an optional Float32 data topic. HealthMonitor monitors both through generic runtime subscriptions.
