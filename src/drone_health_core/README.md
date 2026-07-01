# drone_health_core

Core health, management, and supervision package for the drone health monitoring framework. This package provides the complete backend triad — **Health Monitor**, **Management Node**, and **Supervisor Node** — that work together to give any robotic platform a production-grade safety and diagnostics layer.

> **Note:** All custom message and service definitions (`HealthStatus`, `ManagementState`, `SupervisorStatus`, `RegisterModule`, etc.) live in the separate `drone_health_interfaces` package. This package only contains node implementations and runtime configuration.

---

## 🏗️ System Architecture

The three nodes form a layered pipeline: raw topic diagnostics flow upward into mission/maintenance context, which then flows into a single global Go/No-Go decision.

```mermaid
graph TD
    subgraph Sensors ["Drone / Robot Modules"]
        S1[Lidar]
        S2[Flow / Velocity]
        S3[Camera]
        S4[Network]
    end

    S1 & S2 & S3 & S4 -->|Heartbeats + Data| HM["🩺 Health Monitor<br/>QoS Events + Timeouts"]

    HM -->|/health/status| SUP["🧭 Supervisor Node<br/>Global Decision Engine"]
    MGMT["🛡️ Management Node<br/>Mission / Maintenance / Modules"] -->|/management/state| HM
    MGMT -->|/management/state| SUP
    SafetyFusion["⚠️ Safety Fusion Node<br/>(external pkg)"] -->|/safety/status| SUP
    Net["Network Status"] -->|/network_status| SUP

    SUP -->|/supervisor/status<br/>NORMAL/HOLD/FAILSAFE/E-STOP| Autonomy["Mission Control / Pilot / Dashboard"]
    Autonomy -.->|Register/Deregister/Set Mission| MGMT
```

**Flow Summary:**
1. **Health Monitor** watches every configured and dynamically registered topic, reporting `OK / STALE / ERROR / INACTIVE / UNKNOWN` per topic.
2. **Management Node** owns mission/maintenance state and the module registry, telling Health Monitor which topics are *planned inactive* and telling Supervisor whether a mission is active.
3. **Supervisor Node** fuses Health, Safety, Network, and Management state into one authoritative `SupervisorStatus`, which gates whether any command is allowed to execute.

---

## 📦 Nodes

```text
health_monitor/
  health_monitor_node.cpp
  health_monitor.yaml

management/
  management_node.cpp
  management.yaml

supervisor/
  supervisor_node.cpp
  supervisor.yaml
```

---

## 🔗 Inter-Node Communication

```mermaid
sequenceDiagram
    participant Mod as New Module
    participant Mgmt as Management Node
    participant HM as Health Monitor
    participant Sup as Supervisor Node

    Mod->>Mgmt: register_module(heartbeat, critical=true)
    Mgmt->>Mgmt: Update registry
    Mgmt-->>HM: /management/state (managed_modules)
    HM->>HM: Spawn GenericSubscription for heartbeat
    HM-->>Sup: /health/status (OK)
    Sup->>Sup: Evaluate required_health_topics
    Sup-->>Mod: /supervisor/status (NORMAL, command_allowed=true)

    Mod->>Mgmt: deregister_module(reason=deregistered)
    Mgmt-->>HM: /management/state (planned_inactive_topics/modules)
    HM->>HM: Remove runtime subscriptions for planned-inactive module
    Mgmt-->>Sup: /management/state (planned inactive context)
    Sup-->>Mod: /supervisor/status (mission decision updated)
```

---

## 🧩 Responsibility Split

### 🩺 Health Monitor
| Capability | Detail |
|---|---|
| Static monitoring | YAML-configured typed subscriptions at startup |
| Dynamic monitoring | Runtime `GenericSubscription` spawned from `/management/state` for heartbeat and data topics |
| Fault detection | DDS QoS deadline/liveliness events **+** software timeout fallback (100ms) |
| Mission awareness | Ignores planned-inactive topics/modules so expected downtime is not reported as a failure |
| Output | `/health/status` per-topic health report |

### 🛡️ Management Node
| Capability | Detail |
|---|---|
| Mission control | `mission_active` flag with strict safety interlocks |
| Maintenance mode | Blocks/unblocks topic alerting system-wide |
| Module registry | Static (YAML) + dynamic (`register_module` / `deregister_module` services) |
| Planned inactivity | Tracks per-module/topic reasons (`maintenance`, `optional_disabled`, etc.) |
| Output | `/management/state`, `/management/heartbeat` |

### 🧭 Supervisor Node
| Capability | Detail |
|---|---|
| Fusion | Combines `/safety/status`, `/health/status`, `/network_status`, `/management/state` |
| Failsafe ladder | `UNKNOWN → NORMAL → HOLD → FAILSAFE → EMERGENCY_STOP` |
| Latching | Emergency stop latches on obstacle proximity, requires explicit reset service |
| Network grace period | Distinguishes brief packet loss (`HOLD`) from total loss (`FAILSAFE`) |
| Output | `/supervisor/status` — the single source of truth for `command_allowed` |

---

## 🔄 Combined Decision Flow

```mermaid
stateDiagram-v2
    [*] --> Idle: System Boot
    Idle --> Registering: Modules call register_module
    Registering --> Monitoring: HealthMonitor subscribes dynamically
    Monitoring --> Evaluating: Supervisor fuses Health + Safety + Network
    Evaluating --> NORMAL: All checks pass
    Evaluating --> HOLD: Maintenance / Non-critical fail / Planned inactive
    Evaluating --> FAILSAFE: Stale inputs / Network lost
    Evaluating --> EMERGENCY_STOP: Obstacle too close
    NORMAL --> Evaluating: Continuous 10Hz loop
    EMERGENCY_STOP --> HOLD: reset_emergency_stop (if safe)
```

---

## 🚫 What This Package Does Not Do

This package is intentionally scoped to **diagnostics, lifecycle, and authorization** — not autonomy itself:

- ❌ Mission sequencing / waypoint navigation
- ❌ Return-to-home implementation
- ❌ PX4 / ArduPilot flight control logic
- ❌ Dashboard frontend
- ❌ Simulated sensors

Those belong to separate modules or future robot/autonomy integration that **consumes** `/supervisor/status` as their command-authorization gate.

---

## 🛠️ Build & Run

### Build
```bash
colcon build --packages-select drone_health_core
source install/setup.bash
```

### Run All Three Nodes
```bash
ros2 run drone_health_core management_node --ros-args \
  --params-file install/drone_health_core/share/drone_health_core/management/management.yaml

ros2 run drone_health_core health_monitor_node --ros-args \
  --params-file install/drone_health_core/share/drone_health_core/health_monitor/health_monitor.yaml

ros2 run drone_health_core supervisor_node --ros-args \
  --params-file install/drone_health_core/share/drone_health_core/supervisor/supervisor.yaml
```

### Debug
```bash
ros2 topic echo /management/state
ros2 topic echo /health/status
ros2 topic echo /supervisor/status
```

---

## 🔌 Runtime Registration / Deregistration

Runtime registration allows optional modules to join while ROS is already running. Runtime deregistration allows modules to officially leave without being treated as unexpected failures.

```mermaid
graph LR
    Node["Registrable Node"] -->|register_module / deregister_module| Mgmt["Management Node"]
    Mgmt -->|/management/state| HM["Health Monitor"]
    HM -->|Spawns/Removes Runtime Monitors| Mgmt
    Mgmt -->|/management/state| Sup["Supervisor / Dashboard"]
```

> **Note:** Runtime registration publishes a complete `MonitorSpec[]` for the module. Health Monitor creates generic runtime subscriptions for both heartbeat and data topics, so future message types can be monitored for arrival/timeout without recompiling Health Monitor.

---

## 🎯 Mission Active Semantics

`mission_active` is intentionally a simple high-level boolean:

```text
false = base / idle / preflight
true  = active mission / running task
```

Complex mission behavior (waypoints, state machines, behavior trees, PX4/ArduPilot integration) should be layered **on top of** this package, using `/supervisor/status.command_allowed` as the authorization gate.

---

## 📦 Dependencies

```mermaid
graph LR
    Pkg["drone_health_core"]
    Pkg --> rclcpp
    Pkg --> std_msgs
    Pkg --> std_srvs
```

> Custom message/service types (`HealthStatus`, `SupervisorStatus`, `ManagementState`, `RegisterModule`, etc.) are provided externally by the `drone_health_interfaces` package, which must be built alongside this one.

---

## 📄 License

MIT License. Free to use for academic and commercial robotics projects.
