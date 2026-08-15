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

## Required Adaptation Manifest Format

Every generated or adapted node must include `adaptation_manifest.yaml`.

The manifest must follow this exact schema. Do not use any other layout.

```yaml
profile: "publisher"
ros_distribution: "jazzy"

node:
  name: "<node_name>"
  namespace: "/"
  module_name: "<module_name>"
  language: "<cpp_or_python>"
  lifecycle: false

topics:
  external_inputs: []
  owned_outputs:
    - name: "<output_topic>"
      message_type: "<message_type>"
      kind: "data"
      deadline_ms: 1500
  heartbeat:
    name: "/<module_name>/heartbeat"
    message_type: "std_msgs/msg/String"
    period_ms: 200
    deadline_ms: 500
    liveliness_ms: 0

services:
  register: "/management/register_module"
  deregister: "/management/deregister_module"
  request_deregister: "/<module_name>/request_deregister"

files:
  source:
    - "<relative/source_file>"
  yaml:
    - "<relative/yaml_file>"
  build:
    - "CMakeLists.txt"
  dependencies:
    - "package.xml"
  launch: []
```

Manifest rules:

- `profile` must be a top-level key.
- `ros_distribution` must be a top-level key.
- Do not create a top-level `validator:` key.
- All topic definitions must be under top-level `topics:`.
- Use `topics.owned_outputs[].name`, not `topic`.
- Use `topics.heartbeat.name`, not `topic`.
- `files.source`, `files.yaml`, `files.build`, `files.dependencies`, and `files.launch` must all be lists.
- Paths in `files` must be relative to the adapted package or folder root.

## Standalone Adapted Package Layout

Generated user adaptations must be standalone ROS 2 packages. Do not place new user nodes inside `drone_health_registrable_template`; that package is only the reusable reference template.

Use this layout for generated adaptations:

```text
<node_name>_adapted/
├── src/
│   └── <node_name>.cpp
├── config/
│   └── <node_name>.yaml
├── CMakeLists.txt
├── package.xml
└── adaptation_manifest.yaml
```

For standalone adaptations, `CMakeLists.txt`, `package.xml`, and `adaptation_manifest.yaml` must describe the standalone package name and paths, not `drone_health_registrable_template`.

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
- the complete standalone `CMakeLists.txt`
- the complete standalone `package.xml`
- `adaptation_manifest.yaml`
- run and verification commands

A response is incomplete if it does not include the matching YAML file, complete standalone build files, adaptation manifest, and run/test commands.

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
3. the complete standalone CMakeLists.txt
4. the complete standalone package.xml
5. adaptation_manifest.yaml
6. run and verification commands

Do not stop after only generating C++.
The response is incomplete without YAML, complete standalone build files, adaptation manifest, and run/test commands.
Do not use `drone_health_registrable_template` as the generated package name.
Use `src/<node_name>.cpp` and `config/<node_name>.yaml` for standalone generated files.
Use generic maintainer metadata such as `maintainer@example.com`.

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
  for C++ packages compiled with `-Werror`, store and check the returned Boolean value and log an
  assertion failure when appropriate;
- respect packages that compile with `-Werror`, because ignored return values and compiler warnings
  will fail the build;
- for `rclpy.duration.Duration` on ROS 2 Jazzy, do not use the unsupported `milliseconds=`
  keyword; convert milliseconds with `Duration(nanoseconds=value_ms * 1_000_000)`;
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
11. Adaptation manifest for the generic validator
12. Assumptions and source-file references

The adaptation manifest is validator configuration, not a ROS runtime file. It must describe:

- validator profile and supported ROS distribution;
- ROS node name, namespace, module name, language, and lifecycle status;
- external input topics and their message types;
- owned output topics, message types, monitor kinds, and deadlines;
- heartbeat topic, type, period, deadline, and liveliness;
- registration, deregistration, and request-deregister service names;
- relative paths to adapted source, YAML, build, dependency, and launch files.

The manifest must reflect the generated files exactly. Do not copy topic names, message types, node
names, deadlines, or file paths from an unrelated example.

Example validator command:

```bash
python3 tools/validate_adaptation.py \
  <adaptation-directory> \
  --manifest <adaptation-manifest.yaml>
```

The package-specific YOLO validator may be retained for YOLO experiments, but new adaptations
should use the generic manifest-based validator.

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

After generating the changes, validate in this order:

1. Generic static validation passes using the adaptation manifest
2. Package builds successfully
3. Original node functionality still works
4. Module appears as REGISTERED in /management/state
5. Heartbeat appears in /health/status
6. Each registered owned topic appears in /health/status
7. Dashboard shows the module and topic health
8. Planned deregistration produces PLANNED_INACTIVE
9. Unexpected termination produces STALE or timeout status
10. Original hardware and service behavior remains functional

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


## AI-Assisted Third-Party Processing Package Integration

Use this workflow for existing ROS 2 packages that subscribe to sensor data, process it, and
publish results. Examples include YOLO, image-processing, point-cloud-processing, and
sensor-fusion packages.

### User Responsibilities

Before requesting AI adaptation:

1. Install and run the original third-party package.
2. Confirm that its original inputs, processing, outputs, services, and launch files work without
   Drone Health integration.
3. Clone the matching source version into the ROS 2 workspace.
4. Create a separate integration branch.
5. Identify the external input topics, owned output topics, node names, message types, lifecycle
   behavior, and build files.

Do not adapt a package whose original behavior has not been verified.

### Files to Provide to the AI

Provide only the files required to understand and modify the package:

- processing-node source files;
- relevant headers for C++ packages;
- relevant launch files and existing YAML configuration;
- `CMakeLists.txt` or `setup.py`;
- `package.xml`;
- package documentation that defines important behavior;
- Drone Health interface definitions used by the integration;
- this README and the closest publisher, subscriber, or service template.

Do not include build, install, log, cache, model-weight, dataset, or generated files.

### Processing-Package Rules

The AI must:

- preserve the original processing, hardware, inference, and application logic;
- preserve existing message types, topics, services, parameters, namespaces, launch behavior, and
  lifecycle behavior unless an integration change requires otherwise;
- add only Drone Health registration, heartbeat, deregistration, QoS, parameters, dependencies,
  configuration, and tests;
- use the subscriber-processing pattern when a node consumes input and publishes processed output;
- never register input topics owned by another node;
- register only the heartbeat and data/output topics directly owned by the adapted node;
- never register services as `MonitorSpec` entries;
- keep every `MonitorSpec.message_type` consistent with the real ROS publisher;
- apply configured deadline QoS to the actual monitored publisher;
- keep every deadline greater than the expected publication period;
- preserve ROS 2 lifecycle transitions for lifecycle nodes;
- publish heartbeat only after registration and while the node is active;
- stop or silence health publications during lifecycle deactivation;
- add planned deregistration through a `std_srvs/srv/Trigger` service;
- keep task-based self-deregistration optional;
- omit cycle-based auto-deregistration when the node has no natural completion condition;
- update all required CMake, Python packaging, launch, YAML, and `package.xml` files;
- avoid helper classes, broad rewrites, unrelated exception handling, and duplicated application
  logic;
- return complete files without placeholders or omitted sections;
- state assumptions instead of inventing missing interfaces or behavior.

For Python nodes, translate the integration behavior into idiomatic `rclpy`. Do not copy C++ syntax
or C++-specific implementation details into Python.

### Lifecycle Node Requirements

For a lifecycle node:

- declare and read integration parameters during the appropriate lifecycle phase;
- create publishers, clients, services, and timers without breaking existing lifecycle ownership;
- preserve configure, activate, deactivate, cleanup, and shutdown behavior;
- perform registration only when the node can safely join the framework;
- publish heartbeat and owned output only while active;
- prevent duplicate registration requests;
- preserve retry behavior when Management is unavailable;
- request planned deregistration before an intentional final shutdown;
- do not classify ordinary lifecycle deactivation as unexpected failure unless that is the intended
  framework policy.

### Registration and Lifecycle Ordering

For processing and lifecycle nodes:

- do not publish heartbeat or monitored output before registration succeeds;
- keep processing/output disabled while registration is pending or rejected;
- do not automatically deregister on every lifecycle deactivation;
- ordinary lifecycle deactivation must only stop heartbeat and monitored output;
- use planned deregistration only for an intentional final shutdown requested through
  `request_deregister` or explicit task-completion logic;
- do not destroy timers, clients, or services while an asynchronous registration or
  deregistration request is pending;
- complete planned deregistration before cleanup or shutdown destroys health interfaces;
- avoid calling `rclpy.shutdown()` directly inside an asynchronous service response callback when
  lifecycle shutdown ordering is still required;
- allow lifecycle reactivation without creating duplicate publishers, timers, clients, services,
  or registration requests;
- preserve generic launch defaults; apply package-specific topic overrides in the model-specific
  launch file or YAML;
- validate configured output deadlines using the measured publication rate on the target hardware.

### Recommended Processing-Package Prompt

```text
Adapt this third-party ROS 2 package using the AI-Assisted Third-Party Processing Package
Integration instructions in the template README.

Package type: <publisher, processing, service, lifecycle, or mixed>
External input topics: <topics owned by other nodes>
Owned output topics: <topics published directly by this package>

Preserve the package's original application and lifecycle behavior. Return complete adapted
source, YAML, build, dependency, launch, run, and test content. Do not modify input-topic
ownership or invent framework interfaces.
```

### YOLO Example

For the `yolo_ros` detection node:

```text
Package type: subscriber + publisher lifecycle processing node
External input topic: /image_raw
Owned output topic: /yolo/detections
```

The AI must not register `/image_raw` because the camera node owns it. The adapted YOLO node should
register only its heartbeat and directly published detection output. Debug and tracking outputs
should be registered only when their corresponding nodes are independently adapted and own those
topics.
