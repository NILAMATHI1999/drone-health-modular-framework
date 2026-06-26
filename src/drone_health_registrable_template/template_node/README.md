# registrable_template_node

## Purpose

Reusable ROS 2 template node for modules that need runtime registration and planned deregistration
with the drone health monitoring framework.

Future students can copy this node pattern when adding modules such as camera, GPS, battery,
network, payload, or inspection modules.

## Inputs

- `/management/register_module` (`drone_health_interfaces/srv/RegisterModule`)
- `/management/deregister_module` (`drone_health_interfaces/srv/DeregisterModule`)
- `/template/request_deregister` (`std_srvs/srv/Trigger`)

## Outputs

- `/template/heartbeat` (`std_msgs/msg/String`)

## Parameters

Typical parameters:

- `module_name`
- `critical`
- `heartbeat_topic`
- `publish_period_ms`
- `heartbeat_deadline_ms`
- `heartbeat_liveliness_ms`

## Run command

```bash
ros2 run drone_health_registrable_template registrable_template_node
```

Request planned deregistration:

```bash
ros2 service call /template/request_deregister std_srvs/srv/Trigger
```

## Runtime Registration Flow

```text
node starts
-> calls /management/register_module
-> sends module name, critical flag, heartbeat topic, heartbeat type, and heartbeat QoS timing
-> ManagementNode adds the module to the runtime registry
-> HealthMonitor dynamically monitors the heartbeat
-> dashboard can show the module as active
```

## Runtime Deregistration Flow

```text
operator or node requests planned shutdown
-> node calls /management/deregister_module
-> ManagementNode approves or rejects
-> if approved, node stops publishing and shuts down
-> HealthMonitor shows INACTIVE / DEREGISTERED
```

## Expected Behavior

After startup:

```text
/template/heartbeat -> OK
template_node appears in registry_modules
template_node appears in active_modules
```

After planned deregistration:

```text
template_node remains in registry_modules
template_node is removed from active_modules
template_node appears in planned_inactive_modules
/template/heartbeat appears as INACTIVE / DEREGISTERED
```

## Failure Behavior

Ctrl+C, crash, or power loss does not call deregistration. ManagementNode state does not change and
HealthMonitor reports stale/deadline/liveliness failure.

Planned deregistration asks ManagementNode first and produces `INACTIVE / DEREGISTERED` instead of
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

## Current Limitation

This template dynamically monitors the heartbeat.

Data topics are registered as metadata, but generic runtime data-topic monitoring is future work.
