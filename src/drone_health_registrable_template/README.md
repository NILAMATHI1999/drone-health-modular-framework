# drone_health_registrable_template

Reusable ROS 2 template package for future nodes that need to join the Drone Health Monitoring Framework dynamically.

This package provides three template patterns:

- publisher-style node
- subscriber + publisher-style processing node
- service-style node

Each template shows how a future ROS node can:

- register with Management at startup
- describe monitored topics using `MonitorSpec[]`
- publish a heartbeat
- publish owned data/output topics when needed
- request planned deregistration before shutdown
- optionally self-deregister from internal task logic
- avoid manual edits in HealthMonitor or Management YAML for runtime modules

## Templates

| Template | Executable | Purpose |
|---|---|---|
| Publisher template | `registrable_publisher_template_node` | For nodes that publish data, such as camera, GPS, LiDAR, battery, or network nodes. |
| Subscriber template | `registrable_subscriber_template_node` | For processing nodes that subscribe to input data and publish processed output. |
| Service template | `registrable_service_template_node` | For nodes that provide a ROS service and still need health monitoring through heartbeat. |

## Package Structure

```text
drone_health_registrable_template/
├── config/
│   ├── registrable_publisher_template.yaml
│   ├── registrable_subscriber_template.yaml
│   ├── registrable_service_template.yaml
│   └── cpu_temperature_publisher.yaml
└── template_node/
    ├── registrable_publisher_template_node.cpp
    ├── registrable_subscriber_template_node.cpp
    ├── registrable_service_template_node.cpp
    ├── cpu_temperature_publisher.cpp
    └── README.md
```

## Responsibility Split

| Component | Responsibility |
|---|---|
| Template node | Publishes heartbeat/data, registers MonitorSpecs, and requests deregistration. |
| ManagementNode | Validates registration and publishes runtime module metadata in `/management/state`. |
| HealthMonitor | Reads `/management/state` and creates runtime subscriptions for registered topics. |
| Dashboard | Displays module, health, planned inactive, and rejected registration states. |

## Runtime Registration Flow

```text
template node starts
-> calls /management/register_module
-> sends module_name, critical flag, and MonitorSpec[]
-> ManagementNode validates and stores module
-> ManagementNode publishes /management/state
-> HealthMonitor creates runtime subscriptions
-> Dashboard shows module status
```

## Runtime Deregistration Flow

```text
planned stop requested
-> template node calls /management/deregister_module
-> ManagementNode marks module PLANNED_INACTIVE
-> HealthMonitor removes/silences runtime monitors
-> Dashboard removes active health tiles and shows planned inactive reason
```

## Build

```bash
cd /home/nila/Desktop/drone_health_modular_ws
source /opt/ros/jazzy/setup.bash
colcon build --packages-select drone_health_registrable_template
source install/setup.bash
```

## Run Publisher Template

```bash
ros2 run drone_health_registrable_template registrable_publisher_template_node --ros-args --params-file /home/nila/Desktop/drone_health_modular_ws/src/drone_health_registrable_template/config/registrable_publisher_template.yaml
```

Publishes and registers:

```text
/template_publisher/heartbeat
/template_publisher/value
```

Request planned deregistration:

```bash
ros2 service call /template_publisher/request_deregister std_srvs/srv/Trigger "{}"
```

## Run Subscriber Template

```bash
ros2 run drone_health_registrable_template registrable_subscriber_template_node --ros-args --params-file /home/nila/Desktop/drone_health_modular_ws/src/drone_health_registrable_template/config/registrable_subscriber_template.yaml
```

The subscriber template expects an input topic. For a quick test:

```bash
ros2 topic pub /template/input std_msgs/msg/Float32 "{data: 2.5}" -r 5
```

Publishes and registers:

```text
/template_subscriber/heartbeat
/template_subscriber/output
```

The input topic `/template/input` is not registered by this node because it belongs to the node that publishes it.

Request planned deregistration:

```bash
ros2 service call /template_subscriber/request_deregister std_srvs/srv/Trigger "{}"
```

## Run Service Template

```bash
ros2 run drone_health_registrable_template registrable_service_template_node --ros-args --params-file /home/nila/Desktop/drone_health_modular_ws/src/drone_health_registrable_template/config/registrable_service_template.yaml
```

Test the example service:

```bash
ros2 service call /template_service/run_action std_srvs/srv/Trigger "{}"
```

Publishes and registers:

```text
/template_service/heartbeat
```

The service itself is not registered as a `MonitorSpec` because HealthMonitor monitors topic freshness, not service availability.

Request planned deregistration:

```bash
ros2 service call /template_service/request_deregister std_srvs/srv/Trigger "{}"
```

## Optional Auto Self-Deregistration

Each template includes:

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

Example:

```bash
ros2 run drone_health_registrable_template registrable_publisher_template_node --ros-args --params-file /home/nila/Desktop/drone_health_modular_ws/src/drone_health_registrable_template/config/registrable_publisher_template.yaml -p auto_deregister_after_cycles:=10
```

This lets the node trigger deregistration internally after 10 active timer cycles.

Future users should replace this cycle-count condition with real task logic, such as:

- task complete
- calibration complete
- mission phase finished
- payload no longer required
- optional sensor disabled
- diagnostic task finished

## What Future Students Replace

| Template | Replace |
|---|---|
| Publisher | Data message type, data topic name, and `publish_data()` logic. |
| Subscriber | Input/output message types, `handle_input()` logic, and `publish_output()` logic. |
| Service | Service type, service name, and service callback logic. |

Keep the common integration pattern:

```text
RegisterModule client
MonitorSpec[] creation
heartbeat publisher
DeregisterModule client
planned deregistration flow
```

## AI-Assisted Integration

These templates are designed so a future student or AI tool can adapt an existing ROS package by copying the relevant pattern:

```text
publisher node -> use publisher template pattern
processing node -> use subscriber template pattern
service node -> use service template pattern
mixed node -> combine the relevant patterns
```

Dynamic nodes do not need to be added manually to HealthMonitor YAML or Management YAML. They are discovered from `/management/state` after registration.

The YAML files in `config/` are only parameter examples for these template nodes. They are not core HealthMonitor or Management configuration files.


## YAML Configuration Pattern

Each generated or adapted node should include a matching YAML file.

The top-level YAML key must match the exact ROS node name in the C++ constructor:

```cpp
: Node("registrable_template_node")
```

Example:

```yaml
registrable_template_node:
  ros__parameters:
    module_name: "publisher_template_node"
    critical: false
    heartbeat_topic: "/template_publisher/heartbeat"
    publish_period_ms: 200
    heartbeat_deadline_ms: 500
    heartbeat_liveliness_ms: 0
    publish_data_topic: true
    data_topic: "/template_publisher/value"
    data_deadline_ms: 500
    auto_deregister_after_cycles: 0
    request_deregister_service: "/template_publisher/request_deregister"
```

When adapting this template to a new node, update:

- the top-level key to match `Node("...")`
- `module_name`
- heartbeat topic
- owned data/output topics
- deadline values
- `request_deregister_service`

## AI-Assisted Integration Guidance

When using this template with an AI assistant, keep the prompt strict.

Ask the AI to:

- keep the original ROS node logic compact
- preserve the existing code structure where possible
- only add the drone-health integration blocks
- follow the formatting style of these templates
- avoid unnecessary try/catch blocks, helper classes, or large rewrites

The AI output must include:

- the adapted C++ node
- the matching YAML configuration file
- the required `CMakeLists.txt` additions
- run and verification commands

A response is incomplete if it does not include the matching YAML file, required build changes, and run/test commands

The YAML file must:

- use the exact ROS node name from the C++ constructor as the top-level key
- include every declared ROS parameter
- use topic names that match the registered `MonitorSpec` entries
- keep deadlines greater than the publish period
- include `request_deregister_service`

Recommended prompt:

```text
Use this template as the style reference.
Keep the original node code structure as much as possible.
Only add the required drone-health integration:
- registration with MonitorSpec[]
- heartbeat publisher
- request_deregister service
- deregistration client
- YAML parameters

You must generate all required outputs:
1. the adapted C++ node
2. the matching YAML configuration file
3. the required CMakeLists.txt additions
4. run and verification commands

Do not stop after only generating C++.
The response is incomplete without YAML, CMake additions, and run/test commands

The YAML must use the exact ROS node name from the C++ constructor as the top-level key.

Do not over-format or split simple statements across many lines.
Do not add unrelated try/catch blocks or extra abstractions unless required.
```
