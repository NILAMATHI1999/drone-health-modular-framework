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

template node starts
-> calls /management/register_module
-> sends module_name, critical flag, and MonitorSpec[]
-> ManagementNode validates and stores module
-> ManagementNode publishes /management/state
-> HealthMonitor creates runtime subscriptions
-> Dashboard shows module status
```

## Runtime Deregistration Flow

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

/template_service/heartbeat
```

The service itself is not registered as a `MonitorSpec` because HealthMonitor monitors topic freshness, not service availability.

Request planned deregistration:

```bash
ros2 service call /template_service/request_deregister std_srvs/srv/Trigger "{}"
```

## Optional Auto Self-Deregistration

Each template includes:

auto_deregister_after_cycles
```

Default:

0
```

Meaning:

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

RegisterModule client
MonitorSpec[] creation
heartbeat publisher
DeregisterModule client
planned deregistration flow
```

## AI-Assisted Integration

These templates are designed so a future student or AI tool can adapt an existing ROS package by copying the relevant pattern:

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
## Third-Party Package Integration

The publisher, subscriber-processing, and service templates can also be used to adapt existing third-party
ROS 2 packages.

A separate template is not required for every sensor or package. Select the template according to the
primary role of the node being adapted:

data-producing node       -> publisher template
input-processing node     -> subscriber-processing template
service-oriented node     -> service template
mixed node                -> combine only the required patterns

For example, a USB camera driver primarily publishes camera data. It should therefore use the publisher
template even if it also provides camera-control services.

### User Workflow

The user is responsible for preparing and validating the external package:

1. Install the original package and its dependencies.
2. Connect any required hardware.
3. Run the original package before making changes.
4. Confirm its nodes, topics, services, message types, parameters, and hardware behavior.
5. Identify the installed package version.
6. Clone the matching source version into a ROS 2 workspace.
7. Build and run the unmodified source package.
8. Create a separate integration branch before applying generated changes.
9. Provide the relevant package files and this template documentation to the AI.
10. Review the generated changes before applying them.
11. Build and test the adapted package.
12. Record generated errors, manual corrections, and final test results.

Do not modify packages installed under /opt/ros/<distribution>. Clone a writable source copy instead.

Example preparation:

cd <workspace>/src
git clone --branch <matching-version> <repository-url>
cd <package-directory>
git switch -c drone-health-integration

The exact version should match the package that was successfully tested before adaptation.

### Files to Provide to the AI

Provide the files that define the node and its build/runtime configuration.

Typical C++ package inputs:

relevant include/*.hpp files
relevant src/*.cpp files
configuration YAML files
launch files that start the node
CMakeLists.txt
package.xml
this template README
matching template C++ and YAML files

Typical Python package inputs:

relevant Python node files
setup.py
setup.cfg
package.xml
configuration YAML files
launch files
this template README
matching template implementation

Do not provide generated directories:

build/
install/
log/
.git/

For large repositories, provide only the files related to the node being adapted and the interfaces it
uses. The AI should request missing files rather than assume their contents.

### AI Adaptation Rules

When adapting a third-party ROS 2 package, the AI must:

1. Inspect the supplied source before generating changes.
2. Identify every node being changed.
3. Determine whether each node is primarily a publisher, processing node, service node, or mixed node.
4. Select the matching registrable template pattern.
5. Preserve the original node name, application logic, message types, topics, services, parameters,
   component registration, and hardware behavior.

6. Add only the required Drone Health integration.
7. Register the heartbeat and primary data/output topics owned by the adapted node.
8. Never register input-only topics published by another node.
9. Never register ROS services as MonitorSpec entries.
10. Keep each MonitorSpec.message_type identical to the actual published ROS message type.
11. Apply deadline and reliability settings to the actual publisher QoS, not only to MonitorSpec.
12. Keep every configured deadline greater than the corresponding expected publication period.
13. Add the registration and deregistration service clients using the exact framework paths:

/management/register_module
/management/deregister_module

14. Add a node-specific std_srvs/srv/Trigger service for planned deregistration.
15. Treat cycle-based automatic deregistration as optional demonstration logic. Do not add it as required
   production behavior when the original node has no matching task-completion condition.

16. Update YAML, CMake, package.xml, and launch files only where required.
17. Preserve the package's existing build system and coding style.
18. Do not invent messages, services, packages, parameters, topics, commands, or framework behavior.
19. Do not replace complete source code with comments, pseudocode, or ellipses.
20. State missing information explicitly instead of guessing.

### Additional Validation Rules

When adapting third-party packages, the AI must also:

- choose deadline values from measured or documented publication rates and include sufficient timing
  margin; do not assume the requested sensor rate is always achieved;
- check return values from functions marked `warn_unused_result`, including `assert_liveliness()`;
- respect packages that compile with `-Werror`, because ignored return values and compiler warnings
  will fail the build;
- treat generated deadline values as initial settings that must be verified using measured topic
  frequency;
- never consider an adaptation complete until the target package builds with its original compiler
  options.

### Topic Ownership

Register only the node's primary owned topics.

Examples:

Camera driver:
  register heartbeat
  register image_raw
  register camera_info

Processing node:
  do not register input topic
  register heartbeat
  register processed output

Service-only node:
  register heartbeat
  do not register the service as MonitorSpec

Image-transport or middleware-generated derivative topics should not automatically be registered:

/image_raw/compressed
/image_raw/compressedDepth
/image_raw/theora
/image_raw/zstd

Register such topics only when the adapted node explicitly owns and directly publishes them as part of its
required interface.

### Complex and Mixed Packages

A package may contain multiple nodes. Do not treat the entire package as one module automatically.

For each node:

1. Identify its process and ROS node name.
2. Identify its owned publishers.
3. Identify its subscriptions and provided services.
4. Decide whether it requires separate registration.
5. Use a unique module_name, heartbeat topic, and deregistration service.
6. Generate separate MonitorSpec[] entries for that node's owned topics.

If a package contains reusable libraries and one executable node, modify the node integration layer rather
than the hardware-processing library.

For composable ROS 2 nodes, preserve:

rclcpp::NodeOptions
RCLCPP_COMPONENTS_REGISTER_NODE
component library targets
component executable registration

### Required AI Output

The AI output must include:

1. Complete adapted source files
2. Matching YAML configuration
3. Required CMakeLists.txt changes
4. Required package.xml dependencies
5. Required launch-file changes, if any
6. Build commands
7. Run commands
8. Registration and health verification commands
9. Planned-deregistration test
10. Unexpected-stop and stale-detection test
11. Assumptions and source-file references

For YAML:

- the top-level key must match the effective ROS node name;
- every added parameter must be declared in the node;
- topic names must match the actual publishers and MonitorSpec entries;
- deadline values must be greater than expected publication periods;
- request_deregister_service must be included.

For verification, use interfaces that exist in this framework:

ros2 topic echo /management/state --once
ros2 topic echo /health/status

Do not invent management services such as:

/management/list_modules
/management/get_module_info

Load configuration using:

ros2 run <package> <executable> \
  --ros-args \
  --params-file <absolute-path-to-yaml>

### Validation Sequence

After applying the generated changes, validate in this order:

1. Package builds successfully
2. Original node functionality still works
3. Module appears as REGISTERED in /management/state
4. Heartbeat appears in /health/status
5. Each registered owned topic appears in /health/status
6. Dashboard shows the module and topic health
7. Planned deregistration produces PLANNED_INACTIVE
8. Unexpected termination produces STALE or timeout status
9. Original hardware and service behavior remains functional

An AI-generated adaptation is not considered successful only because code was produced. It must build and
pass the runtime checks above.

### Recommended AI Prompt

Adapt the supplied third-party ROS 2 node to the Drone Health Framework.

Use the supplied registrable template and README as authoritative references.

First inspect the node source, headers, YAML, launch files, CMakeLists.txt, and package.xml.
Preserve the original node logic, topics, message types, parameters, services,
component registration, and hardware behavior.

Add only:
- RegisterModule integration with MonitorSpec[];
- a heartbeat publisher;
- deadline-configured QoS;
- a request_deregister Trigger service;
- DeregisterModule integration;
- matching parameters and YAML;
- required build and package dependencies.

Register only heartbeat and primary topics directly owned by the node.
Do not register subscribed inputs, ROS services, or automatically generated transport topics.
Do not invent project interfaces or commands.

Return complete adapted source files, YAML, CMake/package changes,
build and run commands, health verification commands,
planned-deregistration tests, unexpected-stop tests,
assumptions, and source references.
