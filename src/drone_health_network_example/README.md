# drone_health_network_example

Network monitoring example package for the drone health monitoring framework.

## Purpose

This package contains reusable network-monitoring example nodes ported from the older
`health-monitor-dashboard` workspace.

The nodes publish Wi-Fi, LTE, network-fusion, Huawei HiLink, and serial-AT-template topics that can
be monitored by HealthMonitor and shown in the dashboard.

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
/network/wifi/connected_ssid
/network/wifi/available_ssids
/network/wifi/signal_bars
/network/wifi/link_speed_mbps
/network/wifi/heartbeat
/network/lte/state
/network/lte/operator
/network/lte/rat
/network/lte/rssi_dbm
/network/lte/rsrp_dbm
/network/lte/rsrq_db
/network/lte/sinr_db
/network/lte/heartbeat
/network_status
/network_reason
/network/heartbeat
/network/at_hilink/at_summary
/network/at_lte/state
/network/at_lte/operator
/network/at_lte/rat
/network/at_lte/rssi_dbm
/network/at_lte/rsrp_dbm
/network/at_lte/rsrq_db
/network/at_lte/sinr_db
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

Tune mock AT timing parameters:

```bash
ros2 run drone_health_network_example at_modem_monitor_node --ros-args \
  -p mock_mode:=true \
  -p mock_response_mode:=ok \
  -p poll_period_ms:=2000 \
  -p command_delay_ms:=2000 \
  -p response_timeout_ms:=1000
```

Show fake AT error/timeout cases:

```bash
ros2 run drone_health_network_example at_modem_monitor_node --ros-args \
  -p mock_mode:=true \
  -p mock_response_mode:=error
```

```bash
ros2 run drone_health_network_example at_modem_monitor_node --ros-args \
  -p mock_mode:=true \
  -p mock_response_mode:=timeout
```

Other supported mock response modes:

```text
ok
error
timeout
no_sim
no_service
modem_busy
serial_disconnect
```

## Dashboard Meaning

The dashboard intentionally separates Huawei LTE data from serial AT modem data:

```text
LTE / Huawei tab:
  Source is the Huawei HiLink HTTP API at http://192.168.8.1.
  Values are displayed with AT-style labels such as AT+COPS?, AT+CSQ, and AT^HCSQ.
  These are real Huawei stick values, but they are not read through a serial AT port.

Serial AT tab:
  Source is at_modem_monitor_node.
  With mock_mode:=true, values are fake hardcoded AT-style values for dashboard and health testing.
  Future students can replace the template backend with real /dev/ttyUSB0 serial AT communication.
```

## Mock AT Values

With `mock_mode:=true` and `mock_response_mode:=ok`, `at_modem_monitor_node` publishes fake values:

```text
State: CONNECTED_MOCK
Operator: MOCK_OPERATOR
RAT: LTE/4G
RSSI: -65 dBm
RSRP: -96 dBm
RSRQ: -12 dB
SINR: 2 dB
PLMN: 26202
```

These appear in the dashboard Serial AT tab as:

```text
Mock Serial AT: AT=OK; AT+CSQ=-65 dBm; AT+COPS?=MOCK_OPERATOR; AT^SYSINFOEX=LTE/4G; AT^HCSQ?=-96 dBm,-12 dB,2 dB
```

## AT Timing Parameters

The professor's suggestion to "play with the numbers" means testing different modem timing values:

```text
poll_period_ms       how often the node polls the modem
command_delay_ms     delay between AT commands; start around 2000 ms
response_timeout_ms  time to wait for OK, ERROR, or timeout
```

In mock mode these parameters document and simulate the intended behavior. `mock_response_mode`
lets the dashboard show expected success, error, timeout, no-SIM, no-service, modem-busy, and
serial-disconnect states. Real stability testing requires serial LTE/5G hardware.

## Hardware Notes

`wifi_monitor_node` uses `nmcli`.

`lte_monitor_node` and `at_hilink_adapter_node` expect a Huawei HiLink-style modem available at
`http://192.168.8.1`.

`at_modem_monitor_node` is a mock/template for future serial AT-command modem support. Its real
serial backend is intentionally not implemented yet.

The remaining hardware-dependent work is to implement `send_at_command()` so it opens a serial
port such as `/dev/ttyUSB0`, writes AT commands, waits between commands, reads until `OK`, `ERROR`,
or timeout, handles retries/errors, and returns the raw modem response for parsing.
