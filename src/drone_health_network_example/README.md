# drone_health_network_example

Network monitoring example package for the drone health monitoring framework.

## Purpose

This package contains reusable network-monitoring example nodes ported from the older
`health-monitor-dashboard` workspace.

The nodes publish Wi-Fi, LTE, network-fusion, and modem-template state topics that can be monitored
by HealthMonitor and shown in the dashboard.

## Nodes

- `wifi_monitor_node`
- `lte_monitor_node`
- `network_fusion_node`
- `at_hilink_adapter_node`
- `at_modem_monitor_node`

## Topics

Important outputs:

```text
/network/wifi/state
/network/wifi/heartbeat
/network/lte/state
/network/lte/heartbeat
/network_status
/network_reason
/network/heartbeat
/network/at_hilink/at_summary
/network/at_lte/heartbeat
```

## Build

```bash
colcon build --packages-select drone_health_network_example
```

## Run

```bash
ros2 run drone_health_network_example wifi_monitor_node
```

```bash
ros2 run drone_health_network_example lte_monitor_node
```

```bash
ros2 run drone_health_network_example network_fusion_node
```

```bash
ros2 run drone_health_network_example at_hilink_adapter_node
```

```bash
ros2 run drone_health_network_example at_modem_monitor_node --ros-args -p mock_mode:=true
```

## Hardware Notes

`wifi_monitor_node` uses `nmcli`.

`lte_monitor_node` and `at_hilink_adapter_node` expect a Huawei HiLink-style modem available at
`http://192.168.8.1`.

`at_modem_monitor_node` is a mock/template for future serial AT-command modem support. Its real
serial backend is intentionally not implemented yet.
