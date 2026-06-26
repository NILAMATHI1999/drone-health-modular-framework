# management_node

## Purpose

Central ROS node for high-level system management.

ManagementNode handles operator/module requests such as mission state, maintenance mode, planned
inactive modules, runtime registration, and planned deregistration.

It does not perform safety calculations and does not run mission sequencing.

## Inputs

- `/supervisor/status` (`drone_health_interfaces/msg/SupervisorStatus`)
- service calls from operators, dashboard, or runtime modules
- parameters from `management.yaml`

## Outputs

- `/management/state` (`drone_health_interfaces/msg/ManagementState`)
- `/management/heartbeat` (`std_msgs/msg/String`)

## Parameters

Configured in `management.yaml`.

Important parameters:

- `supervisor_status_topic`
- `supervisor_status_timeout_ms`
- `module_ids`
- `<module>.critical`
- `<module>.topics`

## Run command

```bash
ros2 run drone_health_core management_node --ros-args --params-file /home/nila/Desktop/drone_health_modular_ws/src/drone_health_core/management/management.yaml
```

## Main Services

```text
/management/set_mission_active
/management/set_maintenance_mode
/management/set_module_inactive
/management/register_module
/management/deregister_module
```

## Expected Behavior

ManagementNode publishes the current management state, including:

- `mission_active`
- `maintenance_mode`
- known runtime registry modules
- active modules
- planned inactive modules
- planned inactive topics

Operators and modules should normally use module-level commands. ManagementNode maps modules to
topics using YAML configuration or runtime registration metadata.

## Runtime Registration

Runtime registration allows a new module to join while the ROS system is already running.

A node calls `/management/register_module` with:

- `module_name`
- `critical`
- `heartbeat_topic`
- `heartbeat_type`
- `heartbeat_deadline_ms` or `heartbeat_liveliness_ms`
- optional `data_topics`
- optional `data_topic_types`

ManagementNode stores this in the runtime registry and publishes it through `/management/state`.

## Runtime Deregistration

Runtime deregistration allows a module to officially leave while ROS is running.

A node calls `/management/deregister_module`.

If approved, ManagementNode marks the module as planned inactive with reason `deregistered`.
HealthMonitor then reports `INACTIVE / DEREGISTERED` instead of false failure.

## Failure Behavior

If a critical module is requested inactive or deregistered during an active mission, ManagementNode
rejects the request.

If a module disappears without deregistration, ManagementNode state does not change. HealthMonitor
reports stale/failure.

## Responsibility Boundary

ManagementNode should not contain:

- waypoint logic
- return-home logic
- behavior tree logic
- mission sequencing
- obstacle/speed safety calculation
- dashboard decision logic

Those belong to future autonomy, PX4/ArduPilot, behavior trees, mission systems, SafetyFusion,
Supervisor, or dashboard.

## Management State Meaning

```text
registry_* = known module/topic metadata in current runtime
active_* = currently active / expected online
planned_inactive_* = intentionally inactive
```

Dashboard should show a clean module summary. It does not need to display all topic metadata by
default.
