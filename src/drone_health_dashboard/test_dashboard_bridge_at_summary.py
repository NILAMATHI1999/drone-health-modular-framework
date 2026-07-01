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

    assert dashboard_bridge.build_at_modem_summary(values) == (
        "Mock Serial AT: AT=OK; AT+CSQ=-65 dBm; AT+COPS?=MOCK_OPERATOR; "
        "AT^SYSINFOEX=LTE/4G; AT^HCSQ?=-96 dBm,-12 dB,2 dB"
    )


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

    assert dashboard_bridge.build_at_modem_summary(values) == (
        "Mock Serial AT: AT=TIMEOUT; AT+CSQ=TIMEOUT; AT+COPS?=TIMEOUT; "
        "AT^SYSINFOEX=TIMEOUT; AT^HCSQ?=TIMEOUT,TIMEOUT,TIMEOUT"
    )


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

    assert dashboard_bridge.build_at_modem_summary(values) == (
        "Mock Serial AT: AT=ERROR; AT+CSQ=ERROR; AT+COPS?=ERROR; "
        "AT^SYSINFOEX=ERROR; AT^HCSQ?=ERROR,ERROR,ERROR"
    )


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

    assert dashboard_bridge.build_at_modem_summary(values) == (
        "Serial AT: AT=NO_RESPONSE; AT+CSQ=--; AT+COPS?=--; "
        "AT^SYSINFOEX=--; AT^HCSQ?=--,--,--"
    )


if __name__ == "__main__":
    test_builds_mock_serial_at_summary()
    test_builds_timeout_serial_at_summary()
    test_builds_error_serial_at_summary()
    test_builds_disconnected_serial_at_summary()
