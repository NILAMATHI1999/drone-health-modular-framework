#!/usr/bin/env python3
import importlib.util
from pathlib import Path


module_path = Path(__file__).parent / "scripts" / "dashboard_bridge.py"
spec = importlib.util.spec_from_file_location("dashboard_bridge", module_path)
dashboard_bridge = importlib.util.module_from_spec(spec)
spec.loader.exec_module(dashboard_bridge)


def test_builds_mock_serial_at_summary():
    values = {
        "at_modem_state": "CONNECTED_MOCK",
        "at_modem_operator": "MOCK_OPERATOR from AT+COPS?",
        "at_modem_rat": "LTE/4G from AT^SYSINFOEX",
        "at_modem_rssi": "-65 dBm from AT+CSQ",
        "at_modem_rsrp": "-96 dBm from AT^HCSQ?",
        "at_modem_rsrq": "-12 dB from AT^HCSQ?",
        "at_modem_sinr": "2 dB from AT^HCSQ?",
    }

    assert dashboard_bridge.build_at_modem_summary(values) == "AT=OK"


def test_builds_timeout_serial_at_summary():
    values = {
        "at_modem_state": "TIMEOUT_MOCK",
        "at_modem_operator": "TIMEOUT from AT+COPS?",
        "at_modem_rat": "TIMEOUT from AT^SYSINFOEX",
        "at_modem_rssi": "TIMEOUT from AT+CSQ",
        "at_modem_rsrp": "TIMEOUT from AT^HCSQ?",
        "at_modem_rsrq": "TIMEOUT from AT^HCSQ?",
        "at_modem_sinr": "TIMEOUT from AT^HCSQ?",
    }

    assert dashboard_bridge.build_at_modem_summary(values) == "AT=TIMEOUT"


def test_builds_error_serial_at_summary():
    values = {
        "at_modem_state": "ERROR_MOCK",
        "at_modem_operator": "ERROR from AT+COPS?",
        "at_modem_rat": "ERROR from AT^SYSINFOEX",
        "at_modem_rssi": "ERROR from AT+CSQ",
        "at_modem_rsrp": "ERROR from AT^HCSQ?",
        "at_modem_rsrq": "ERROR from AT^HCSQ?",
        "at_modem_sinr": "ERROR from AT^HCSQ?",
    }

    assert dashboard_bridge.build_at_modem_summary(values) == "AT=ERROR"


def test_builds_disconnected_serial_at_summary():
    values = {
        "at_modem_state": "DISCONNECTED",
        "at_modem_operator": "--",
        "at_modem_rat": "--",
        "at_modem_rssi": "--",
        "at_modem_rsrp": "--",
        "at_modem_rsrq": "--",
        "at_modem_sinr": "--",
    }

    assert dashboard_bridge.build_at_modem_summary(values) == "AT=NO_RESPONSE"


if __name__ == "__main__":
    test_builds_mock_serial_at_summary()
    test_builds_timeout_serial_at_summary()
    test_builds_error_serial_at_summary()
    test_builds_disconnected_serial_at_summary()
