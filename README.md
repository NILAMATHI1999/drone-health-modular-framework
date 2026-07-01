# Drone Health Monitoring Framework

[![ROS 2](https://img.shields.io/badge/ROS_2-Jazzy-blue)](https://docs.ros.org/)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-purple.svg)](https://en.cppreference.com/w/cpp/17)
[![Python3](https://img.shields.io/badge/Python-3.8+-blue.svg)](https://www.python.org/)

A comprehensive, modular ROS 2 framework for autonomous drone health monitoring, system management, and safety supervision. This framework provides a complete closed-loop safety architecture — from sensor data ingestion to global system authorization — with runtime module registration, planned deregistration, and a live web dashboard.

---

## 🏗️ System Architecture

```mermaid
graph TD

    subgraph PKG_INTERFACES ["📦 drone_health_interfaces"]
        MSG1["HealthStatus.msg"]
        MSG2["ManagementState.msg"]
        MSG3["MonitorSpec.msg"]
        SRV1["RegisterModule.srv"]
        SRV2["DeregisterModule.srv"]
    end

    subgraph PKG_LIDAR ["📦 drone_health_lidar_example"]
        LIDAR["simulated_lidar_driver_node<br/>/lidar/scan, /lidar/heartbeat"]
        LPROC["lidar_obstacle_processor_node<br/>/lidar/nearest_obstacle,<br/>/lidar_obstacle_processor/heartbeat"]
    end

    subgraph PKG_FLOW ["📦 drone_health_flow_example"]
        FLOW["simulated_flow_sensor_node<br/>/vehicle/velocity, /flow/heartbeat"]
    end

    subgraph PKG_SAFETY ["📦 drone_health_safety_example"]
        SF["safety_fusion_node<br/>Kinematic Braking Math<br/>/safety_fusion/heartbeat"]
    end

    subgraph PKG_CORE ["📦 drone_health_core"]
        MN["management_node<br/>Module Registry + State Machine<br/>/management/state"]
        HM["health_monitor_node<br/>QoS + Timeout Engine<br/>/health/status"]
        SV["supervisor_node<br/>Go/No-Go Decision<br/>/supervisor/heartbeat"]
    end

    subgraph PKG_NETWORK ["📦 drone_health_network_example"]
        WIFI["wifi_monitor_node<br/>/network/wifi/heartbeat"]
        LTE["lte_monitor_node<br/>/network/lte/heartbeat"]
        HILINK["at_hilink_adapter_node<br/>/network/at_hilink/heartbeat"]
        MODEM["at_modem_monitor_node<br/>/network/at_lte/heartbeat"]
        NF["network_fusion_node<br/>/network/status, /network/heartbeat"]
    end

    subgraph PKG_EXAMPLES ["📦 drone_health_examples"]
        MM["mission_manager_node<br/>Flight Executive"]
        CAM["simulated_camera_node<br/>/camera/image_raw,<br/>/camera/heartbeat"]
    end

    subgraph PKG_TEMPLATE ["📦 drone_health_registrable_template"]
        TEMPLATE["registrable_template_node<br/>Runtime Registration Demo"]
    end

    subgraph PKG_DASHBOARD ["📦 drone_health_dashboard"]
        DASH["dashboard_bridge.py<br/>SSE Stream on :8080"]
    end

    %% ---- Sensor to Processing ----
    LIDAR --> LPROC

    %% ---- Processing/Sensors to Safety ----
    LPROC --> SF
    FLOW --> SF

    %% ---- Safety to Supervisor ----
    SF --> SV

    %% ---- Management orchestration ----
    MN --> HM
    MN --> SV
    MN --> SF
    MM --> MN
    CAM --> MN
    TEMPLATE --> MN

    %% ---- Network fusion to Supervisor ----
    NF --> SV
    WIFI --> NF
    LTE --> NF
    HILINK --> NF
    MODEM --> NF

    %% ---- Everything reports health status to HM ----
    LIDAR --> HM
    FLOW --> HM
    LPROC --> HM
    SF --> HM
    SV --> HM
    NF --> HM
    WIFI --> HM
    LTE --> HM
    HILINK --> HM
    MODEM --> HM
    CAM --> HM
    TEMPLATE --> HM

    %% ---- Dashboard aggregates everything ----
    SV --> DASH
    SF --> DASH
    HM --> DASH
    MN --> DASH
    NF --> DASH

    %% ---- Interfaces dependency (compile-time, dashed) ----
    PKG_INTERFACES -.->|msg/srv types| PKG_CORE
    PKG_INTERFACES -.->|msg/srv types| PKG_LIDAR
    PKG_INTERFACES -.->|msg/srv types| PKG_FLOW
    PKG_INTERFACES -.->|msg/srv types| PKG_SAFETY
    PKG_INTERFACES -.->|msg/srv types| PKG_NETWORK
    PKG_INTERFACES -.->|msg/srv types| PKG_EXAMPLES
    PKG_INTERFACES -.->|msg/srv types| PKG_TEMPLATE

    style PKG_INTERFACES fill:#fff3cd,stroke:#856404
    style PKG_CORE fill:#d1ecf1,stroke:#0c5460
    style PKG_DASHBOARD fill:#e2d9f3,stroke:#5a3d99
```

---

## 📦 Package Overview

| Package | Purpose |
|---|---|
| `drone_health_interfaces` | Shared messages and services used by all packages |
| `drone_health_core` | Core nodes: Management, Health Monitor, Supervisor |
| `drone_health_safety_example` | Safety Fusion Node — kinematic braking clearance |
| `drone_health_lidar_example` | Simulated LiDAR driver + obstacle processor |
| `drone_health_flow_example` | Simulated flow/velocity sensor |
| `drone_health_network_example` | WiFi, LTE, AT modem monitors + network fusion |
| `drone_health_examples` | Mission manager + simulated camera with deregistration |
| `drone_health_registrable_template` | Reusable template for runtime-registered modules |
| `drone_health_dashboard` | Web dashboard with live SSE streaming |

---

## 🚀 Complete Launch Sequence

### 0. Prerequisites

```bash
cd /home/nila/Desktop/drone_health_modular_ws
source /opt/ros/jazzy/setup.bash
source install/setup.bash
```

### 1. Core Infrastructure (Start First)

```bash
# Terminal 1: Management Node
ros2 run drone_health_core management_node --ros-args --params-file \
  src/drone_health_core/management/management.yaml

# Terminal 2: Health Monitor
ros2 run drone_health_core health_monitor_node --ros-args --params-file \
  src/drone_health_core/health_monitor/health_monitor.yaml

# Terminal 3: Supervisor
ros2 run drone_health_core supervisor_node --ros-args --params-file \
  src/drone_health_core/supervisor/supervisor.yaml
```

### 2. Sensor Layer

```bash
# Terminal 4: LiDAR Driver
ros2 run drone_health_lidar_example simulated_lidar_driver_node

# Terminal 5: LiDAR Processor
ros2 run drone_health_lidar_example lidar_obstacle_processor_node

# Terminal 6: Flow Sensor
ros2 run drone_health_flow_example simulated_flow_sensor_node \
  --ros-args -p simulate_motion:=true
```

### 3. Safety Fusion

```bash
# Terminal 7: Safety Fusion Node
ros2 run drone_health_safety_example safety_fusion_node --ros-args --params-file \
  src/drone_health_safety_example/safety_fusion/safety_fusion.yaml
```

### 4. Network Monitoring (Optional)

```bash
# Terminal 8: WiFi Monitor
ros2 run drone_health_network_example wifi_monitor_node

# Terminal 9: LTE Monitor
ros2 run drone_health_network_example lte_monitor_node

# Terminal 10: Network Fusion
ros2 run drone_health_network_example network_fusion_node

# Terminal 11: AT-HiLink Adapter
ros2 run drone_health_network_example at_hilink_adapter_node

# Terminal 12: AT Modem Monitor (Mock Mode)
ros2 run drone_health_network_example at_modem_monitor_node \
  --ros-args -p mock_mode:=true
```

### 5. Optional Demo Modules

```bash
# Terminal 13: Registrable Template Node
ros2 run drone_health_registrable_template registrable_template_node

# Terminal 14: Simulated Camera
ros2 run drone_health_examples simulated_camera_node

# Terminal 15: Mission Manager (or use terminal commands below)
ros2 run drone_health_examples mission_manager_node \
  --ros-args -p start_delay_s:=5 -p inspection_duration_s:=10
```

### 6. Dashboard

```bash
# Terminal 16: Dashboard Bridge
ros2 run drone_health_dashboard dashboard_bridge.py
```

Open in browser: **http://localhost:8080**

---

## 🧪 Test Cases & Manual Commands

### Mission State Control

```bash
# Activate mission
ros2 service call /management/set_mission_active std_srvs/srv/SetBool "{data: true}"

# Deactivate mission
ros2 service call /management/set_mission_active std_srvs/srv/SetBool "{data: false}"

# Toggle maintenance mode
ros2 service call /management/set_maintenance_mode std_srvs/srv/SetBool "{data: true}"
ros2 service call /management/set_maintenance_mode std_srvs/srv/SetBool "{data: false}"
```

### Template Node — Runtime Registration & Deregistration

```bash
# Self-deregister via template's own service
ros2 service call /template/request_deregister std_srvs/srv/Trigger "{}"

# Operator-triggered deregistration (for deadline/stale testing)
ros2 service call /management/deregister_module drone_health_interfaces/srv/DeregisterModule \
  "{module_name: template_node, reason: deregistered}"
```

### Camera Node — Self-Deregistration & Restore

```bash
# Self-deregister via camera's own service
ros2 service call /camera/request_deregister std_srvs/srv/Trigger "{}"

# Operator-triggered deregistration
ros2 service call /management/deregister_module drone_health_interfaces/srv/DeregisterModule \
  "{module_name: 'camera', reason: 'deregistered'}"

# Restore camera to active state
ros2 service call /management/set_module_inactive drone_health_interfaces/srv/SetModuleInactive \
  "{module_name: 'camera', inactive: false, reason: 'deregistered'}"
```

---

## 🔄 Data Flow Summary

```mermaid
graph LR
    LIDAR["LiDAR"] --> OBS["/lidar/nearest_obstacle"]
    FLOW["Flow Sensor"] --> VEL["/vehicle/velocity"]
    OBS --> SF[Safety Fusion]
    VEL --> SF
    SF --> SV[Supervisor]
    HM[Health Monitor] --> SV
    MN[Management] --> HM
    MN --> SV
    NF[Network Fusion] --> SV
    SV --> DASH[Dashboard]
```

---

## 📄 License

MIT License. Free to use for academic and commercial robotics projects.
