# mission_manager_node

## Purpose

Simple optional mission manager demo.

It can request `mission_active=true` from ManagementNode and publish mission phase messages for
testing optional module behavior.

This node is not part of the core system. A real robot may replace it with an autonomy stack,
PX4/ArduPilot integration, behavior tree, state machine, or operator command interface.

## Inputs

None in the simple demo version.

## Outputs

- `/mission/phase` (`std_msgs/msg/String`)
- service calls to `/management/set_mission_active` (`std_srvs/srv/SetBool`)

## Parameters

Configured by node parameters if needed.

## Run command

```bash
ros2 run drone_health_examples mission_manager_node
```

## Expected Behavior

The node can demonstrate a basic sequence:

```text
start mission
publish inspection phase
publish inspection complete
keep or stop mission active depending on demo configuration
```

## Failure Behavior

If this node is not running, the core system still works.

Mission state can still be controlled directly through ManagementNode:

```bash
ros2 service call /management/set_mission_active std_srvs/srv/SetBool "{data: true}"
```

## Real System Meaning

This is only a teaching/demo node.

In a real drone, mission state usually comes from an operator command, autonomy stack, PX4/ArduPilot
mission state, behavior tree, or state machine.
