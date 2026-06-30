#!/usr/bin/env python3
import json
import math
import mimetypes
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

import rclpy
from ament_index_python.packages import get_package_share_directory
from drone_health_interfaces.msg import (
    HealthStatus,
    ManagementState,
    SafetyStatus,
    SupervisorStatus,
)
from geometry_msgs.msg import TwistStamped
from rclpy.node import Node
from rclpy.qos import HistoryPolicy, QoSProfile, ReliabilityPolicy
from std_msgs.msg import Float32, Int32, String


class DashboardBridgeNode(Node):
    def __init__(self):
        super().__init__("dashboard_bridge_node")

        self.declare_parameter("web_port", 8080)
        self.web_port = self.get_parameter("web_port").value

        self.lock = threading.Lock()
        self.state = {
            "supervisor": {
                "mode": "NO_DATA",
                "reason": "NO_DATA",
                "message": "waiting for supervisor status",
                "command_allowed": False,
                "color": "yellow",
            },
            "safety": {
                "state": "NO_DATA",
                "reason": "NO_DATA",
                "color": "yellow",
                "nearest_obstacle_m": None,
                "speed_mps": None,
                "braking_distance_m": None,
                "required_clearance_m": None,
            },
            "management": {
                "mission_active": False,
                "maintenance_mode": False,
                "reason": "NO_DATA",
                "message": "no management data",
                "managed_modules": [],
                "planned_inactive_modules": [],
                "planned_inactive_module_reasons": [],
                "planned_inactive_topics": [],
                "planned_inactive_topic_reasons": [],
                "rejected_modules": [],
                "rejected_module_reasons": [],
            },
            "metrics": {
                "nearest_obstacle_m": None,
                "speed_mps": None,
                "velocity_x_mps": None,
                "velocity_y_mps": None,
                "velocity_z_mps": None,
            },
            "network": {
                "status": "NO_DATA",
                "reason": "NO_DATA",
                "wifi_state": "NO_DATA",
                "wifi_ssid": "--",
                "wifi_available_ssids": "--",
                "wifi_signal_bars": None,
                "wifi_speed_mbps": None,
                "lte_state": "NO_DATA",
                "lte_operator": "--",
                "lte_rat": "--",
                "lte_rssi": "--",
                "lte_rsrp": "--",
                "lte_rsrq": "--",
                "lte_sinr": "--",
                "at_summary": "--",
            },
            "health": {},
            "events": [],
            "health_stale_event_sent": False,
            "last_seen": {
                "supervisor": None,
                "safety": None,
                "health": None,
                "management": None,
                "nearest_obstacle": None,
                "velocity": None,
                "network": None,

            },
        }

        velocity_qos = QoSProfile(
            history=HistoryPolicy.KEEP_LAST,
            depth=10,
            reliability=ReliabilityPolicy.BEST_EFFORT,
        )

        self.create_subscription(HealthStatus, "/health/status", self.handle_health, 10)
        self.create_subscription(SafetyStatus, "/safety/status", self.handle_safety, 10)
        self.create_subscription(
            SupervisorStatus,
            "/supervisor/status",
            self.handle_supervisor,
            10,
        )
        self.create_subscription(
            ManagementState,
            "/management/state",
            self.handle_management,
            10,
        )
        self.create_subscription(Float32, "/lidar/nearest_obstacle", self.handle_nearest, 10)
        self.create_subscription(
            TwistStamped,
            "/vehicle/velocity",
            self.handle_velocity,
            velocity_qos,
        )
        self.create_subscription(String, "/network_status", self.handle_network_status, 10)
        self.create_subscription(String, "/network_reason", self.handle_network_reason, 10)
        self.create_subscription(String, "/network/wifi/state", self.handle_wifi_state, 10)
        self.create_subscription(String, "/network/wifi/connected_ssid", self.handle_wifi_ssid, 10)
        self.create_subscription(String, "/network/wifi/available_ssids", self.handle_wifi_available, 10)
        self.create_subscription(Int32, "/network/wifi/signal_bars", self.handle_wifi_signal_bars, 10)
        self.create_subscription(Int32, "/network/wifi/link_speed_mbps", self.handle_wifi_speed, 10)
        self.create_subscription(String, "/network/lte/state", self.handle_lte_state, 10)
        self.create_subscription(String, "/network/lte/operator", self.handle_lte_operator, 10)
        self.create_subscription(String, "/network/lte/rat", self.handle_lte_rat, 10)
        self.create_subscription(String, "/network/lte/rssi_dbm", self.handle_lte_rssi, 10)
        self.create_subscription(String, "/network/lte/rsrp_dbm", self.handle_lte_rsrp, 10)
        self.create_subscription(String, "/network/lte/rsrq_db", self.handle_lte_rsrq, 10)
        self.create_subscription(String, "/network/lte/sinr_db", self.handle_lte_sinr, 10)
        self.create_subscription(String, "/network/at_hilink/at_summary", self.handle_at_summary, 10)

        self.web_root = Path(get_package_share_directory("drone_health_dashboard")) / "web"
        self.httpd = self.make_server()
        self.server_thread = threading.Thread(target=self.httpd.serve_forever, daemon=True)
        self.server_thread.start()

        self.get_logger().info(f"Dashboard available at http://localhost:{self.web_port}")

    def make_server(self):
        node = self

        class Handler(BaseHTTPRequestHandler):
            def do_GET(self):
                if self.path == "/events":
                    self.send_response(200)
                    self.send_header("Content-Type", "text/event-stream")
                    self.send_header("Cache-Control", "no-cache")
                    self.send_header("Connection", "keep-alive")
                    self.end_headers()

                    while rclpy.ok():
                        try:
                            payload = json.dumps(node.snapshot())
                            self.wfile.write(f"data: {payload}\n\n".encode("utf-8"))
                            self.wfile.flush()
                            time.sleep(0.5)
                        except (BrokenPipeError, ConnectionResetError):
                            break
                    return

                requested = self.path.split("?", 1)[0]
                if requested == "/":
                    requested = "/index.html"

                file_path = (node.web_root / requested.lstrip("/")).resolve()

                if not str(file_path).startswith(str(node.web_root.resolve())):
                    self.send_error(404)
                    return

                if not file_path.exists():
                    self.send_error(404)
                    return

                content_type = mimetypes.guess_type(file_path.name)[0]
                if content_type is None:
                    content_type = "application/octet-stream"

                self.send_response(200)
                self.send_header("Content-Type", content_type)
                self.end_headers()
                try:
                    self.wfile.write(file_path.read_bytes())
                except (BrokenPipeError, ConnectionResetError):
                    return


            def log_message(self, format, *args):
                return

        return ThreadingHTTPServer(("0.0.0.0", int(self.web_port)), Handler)

    def snapshot(self):
        with self.lock:
            snapshot = json.loads(json.dumps(self.state))

        now = time.time()
        supervisor_seen = snapshot["last_seen"]["supervisor"]
        safety_seen = snapshot["last_seen"]["safety"]
        health_seen = snapshot["last_seen"].get("health")
        management_seen = snapshot["last_seen"].get("management")
        nearest_seen = snapshot["last_seen"].get("nearest_obstacle")
        velocity_seen = snapshot["last_seen"].get("velocity")
        network_seen = snapshot["last_seen"].get("network")

        if supervisor_seen is None:
            snapshot["supervisor"] = {
                "mode": "NO DATA",
                "reason": "WAITING_FOR_SUPERVISOR",
                "message": "waiting for supervisor status",
                "command_allowed": False,
                "color": "yellow",
            }
        elif now - supervisor_seen > 1.5:
            snapshot["supervisor"] = {
                "mode": "STALE",
                "reason": "SUPERVISOR_STATUS_STALE",
                "message": "supervisor status is not updating",
                "command_allowed": False,
                "color": "red",
            }

        if safety_seen is None:
            snapshot["safety"] = {
                "state": "NO DATA",
                "reason": "WAITING_FOR_SAFETY",
                "color": "yellow",
                "nearest_obstacle_m": None,
                "speed_mps": None,
                "braking_distance_m": None,
                "required_clearance_m": None,
            }
        elif now - safety_seen > 1.5:
            snapshot["safety"] = {
                "state": "STALE",
                "reason": "SAFETY_STATUS_STALE",
                "color": "red",
                "nearest_obstacle_m": None,
                "speed_mps": None,
                "braking_distance_m": None,
                "required_clearance_m": None,
            }

        if management_seen is None:
            snapshot["management"] = {
                "mission_active": False,
                "maintenance_mode": False,
                "reason": "NO_DATA",
                "message": "waiting for management state",
                "managed_modules": [],
                "planned_inactive_modules": [],
                "planned_inactive_module_reasons": [],
                "planned_inactive_topics": [],
                "planned_inactive_topic_reasons": [],
                "rejected_modules": [],
                "rejected_module_reasons": [],
            }
        elif now - management_seen > 2.0:
            snapshot["management"]["reason"] = "STALE"
            snapshot["management"]["message"] = "management state is not updating"

        if nearest_seen is not None and now - nearest_seen > 1.5:
            snapshot["metrics"]["nearest_obstacle_m"] = None

        if velocity_seen is not None and now - velocity_seen > 1.5:
            snapshot["metrics"]["speed_mps"] = None
            snapshot["metrics"]["velocity_x_mps"] = None
            snapshot["metrics"]["velocity_y_mps"] = None
            snapshot["metrics"]["velocity_z_mps"] = None

        if network_seen is None:
            snapshot["network"]["status"] = "NO_DATA"
            snapshot["network"]["reason"] = "waiting for network status"
        elif now - network_seen > 3.0:
            snapshot["network"]["status"] = "STALE"
            snapshot["network"]["reason"] = "network data is not updating"

        if health_seen is not None and now - health_seen > 2.0:
            for item in snapshot["health"].values():
                item["status"] = "STALE"
                item["reason"] = "NO_RECENT_HEALTH_UPDATE"
                item["message"] = "health monitor data is not updating"
                item["color"] = "gray"

            with self.lock:
                if not self.state["health_stale_event_sent"]:
                    self.add_event("HealthMonitor data stale", "gray")
                    self.state["health_stale_event_sent"] = True

        return snapshot

    def add_event(self, label, color):
        self.state["events"].insert(0, {
            "time": time.strftime("%H:%M:%S"),
            "label": label,
            "color": color,
        })
        self.state["events"] = self.state["events"][:8]

    def handle_health(self, msg):
        status = self.health_status_text(msg.status)
        reason = self.health_reason_text(msg.reason)
        color = self.health_color(msg.status)
        key = msg.topic_name or msg.node_name

        with self.lock:
            management = self.state.get("management", {})
            inactive_modules = set(management.get("planned_inactive_modules", []))
            inactive_topics = set(management.get("planned_inactive_topics", []))

            if msg.node_name in inactive_modules or msg.topic_name in inactive_topics:
                self.state["health"].pop(key, None)
                return

            self.state["last_seen"]["health"] = time.time()
            self.state["health_stale_event_sent"] = False
            old_status = self.state["health"].get(key, {}).get("status")
            old_reason = self.state["health"].get(key, {}).get("reason")
            self.state["health"][key] = {
                "node_name": msg.node_name,
                "topic_name": msg.topic_name,
                "status": status,
                "reason": reason,
                "message": msg.message,
                "age_s": round(float(msg.last_update_age_s), 2),
                "color": color,
            }

            if old_status and (old_status != status or old_reason != reason):
                self.add_event(f"{msg.topic_name}: {status} / {reason}", color)

    def safe_number(self, value):
        value = float(value)
        if not math.isfinite(value):
            return None
        return round(value, 2)

    def handle_safety(self, msg):
        with self.lock:
            self.state["last_seen"]["safety"] = time.time()
            self.state["safety"] = {
                "state": self.safety_state_text(msg.state),
                "reason": self.safety_reason_text(msg.reason),
                "color": self.safety_color(msg.state),
                "nearest_obstacle_m": self.safe_number(msg.nearest_obstacle_m),
                "speed_mps": self.safe_number(msg.speed_mps),
                "braking_distance_m": self.safe_number(msg.braking_distance_m),
                "required_clearance_m": self.safe_number(msg.required_clearance_m),
            }

    def handle_management(self, msg):
        reason = self.management_reason_text(msg.reason)

        with self.lock:
            self.state["last_seen"]["management"] = time.time()
            old_message = self.state["management"].get("message")
            self.state["management"] = {
                "mission_active": bool(msg.mission_active),
                "maintenance_mode": bool(msg.maintenance_mode),
                "reason": reason,
                "message": msg.message,
                "managed_modules": [
                    {
                        "module_name": module.module_name,
                        "critical": bool(module.critical),
                        "state": module.state,
                        "last_reason": module.last_reason,
                        "monitors": [
                            {
                                "topic_name": monitor.topic_name,
                                "kind": monitor.kind,
                                "message_type": monitor.message_type,
                                "reliability": monitor.reliability,
                                "deadline_ms": int(monitor.deadline_ms),
                                "liveliness_ms": int(monitor.liveliness_ms),
                            }
                            for monitor in module.monitors
                        ],
                    }
                    for module in msg.managed_modules
                ],
                "planned_inactive_modules": list(msg.planned_inactive_modules),
                "planned_inactive_module_reasons": list(msg.planned_inactive_module_reasons),
                "planned_inactive_topics": list(msg.planned_inactive_topics),
                "planned_inactive_topic_reasons": list(msg.planned_inactive_topic_reasons),
                "rejected_modules": list(msg.rejected_modules),
                "rejected_module_reasons": list(msg.rejected_module_reasons),
            }

            inactive_modules = set(msg.planned_inactive_modules)
            inactive_topics = set(msg.planned_inactive_topics)
            removed_health = []

            for key, health in list(self.state["health"].items()):
                if (
                    health.get("node_name") in inactive_modules
                    or health.get("topic_name") in inactive_topics
                ):
                    removed_health.append(health.get("topic_name") or health.get("node_name") or key)
                    del self.state["health"][key]

            if removed_health:
                self.add_event(
                    "Health tiles removed for planned inactive: " +
                    ", ".join(sorted(removed_health)[:3]),
                    "gray",
                )

            if old_message and old_message != msg.message:
                self.add_event(f"Management: {msg.message}", "gray")

    def handle_supervisor(self, msg):
        mode = self.supervisor_mode_text(msg.mode)
        reason = self.supervisor_reason_text(msg.reason)
        color = self.supervisor_color(msg.mode)

        with self.lock:
            self.state["last_seen"]["supervisor"] = time.time()
            old_mode = self.state["supervisor"].get("mode")
            self.state["supervisor"] = {
                "mode": mode,
                "reason": reason,
                "message": msg.message,
                "command_allowed": bool(msg.command_allowed),
                "color": color,
            }

            if old_mode and old_mode != mode:
                self.add_event(f"Supervisor: {mode}", color)

    def handle_nearest(self, msg):
        with self.lock:
            self.state["last_seen"]["nearest_obstacle"] = time.time()
            self.state["metrics"]["nearest_obstacle_m"] = self.safe_number(msg.data)

    def handle_velocity(self, msg):
        linear = msg.twist.linear
        speed = (linear.x ** 2 + linear.y ** 2 + linear.z ** 2) ** 0.5

        with self.lock:
            self.state["last_seen"]["velocity"] = time.time()
            self.state["metrics"]["speed_mps"] = self.safe_number(speed)
            self.state["metrics"]["velocity_x_mps"] = self.safe_number(linear.x)
            self.state["metrics"]["velocity_y_mps"] = self.safe_number(linear.y)
            self.state["metrics"]["velocity_z_mps"] = self.safe_number(linear.z)

    def set_network_value(self, key, value):
        with self.lock:
            self.state["last_seen"]["network"] = time.time()
            self.state["network"][key] = value

    def handle_network_status(self, msg):
        self.set_network_value("status", msg.data)

    def handle_network_reason(self, msg):
        self.set_network_value("reason", msg.data)

    def handle_wifi_state(self, msg):
        self.set_network_value("wifi_state", msg.data)

    def handle_wifi_ssid(self, msg):
        self.set_network_value("wifi_ssid", msg.data)

    def handle_wifi_available(self, msg):
        self.set_network_value("wifi_available_ssids", msg.data or "--")

    def handle_wifi_signal_bars(self, msg):
        self.set_network_value("wifi_signal_bars", int(msg.data))

    def handle_wifi_speed(self, msg):
        self.set_network_value("wifi_speed_mbps", int(msg.data))

    def handle_lte_state(self, msg):
        self.set_network_value("lte_state", msg.data)

    def handle_lte_operator(self, msg):
        self.set_network_value("lte_operator", msg.data)

    def handle_lte_rat(self, msg):
        self.set_network_value("lte_rat", msg.data)

    def handle_lte_rssi(self, msg):
        self.set_network_value("lte_rssi", msg.data)

    def handle_lte_rsrp(self, msg):
        self.set_network_value("lte_rsrp", msg.data)

    def handle_lte_rsrq(self, msg):
        self.set_network_value("lte_rsrq", msg.data)

    def handle_lte_sinr(self, msg):
        self.set_network_value("lte_sinr", msg.data)

    def handle_at_summary(self, msg):
        self.set_network_value("at_summary", msg.data)

    def health_status_text(self, value):
        return {
            HealthStatus.OK: "OK",
            HealthStatus.WARNING: "WARNING",
            HealthStatus.ERROR: "ERROR",
            HealthStatus.STALE: "STALE",
            HealthStatus.UNKNOWN: "UNKNOWN",
            HealthStatus.INACTIVE: "INACTIVE",
        }.get(value, f"UNKNOWN_{value}")

    def health_reason_text(self, value):
        return {
            HealthStatus.REASON_NONE: "NONE",
            HealthStatus.REASON_DEADLINE_MISSED: "DEADLINE_MISSED",
            HealthStatus.REASON_LIVELINESS_LOST: "LIVELINESS_LOST",
            HealthStatus.REASON_HEARTBEAT_TIMEOUT: "HEARTBEAT_TIMEOUT",
            HealthStatus.REASON_QOS_INCOMPATIBLE: "QOS_INCOMPATIBLE",
            HealthStatus.REASON_NODE_NOT_REGISTERED: "NODE_NOT_REGISTERED",
            HealthStatus.REASON_MESSAGE_TIMEOUT: "MESSAGE_TIMEOUT",
            HealthStatus.REASON_MAINTENANCE: "MAINTENANCE",
            HealthStatus.REASON_DEREGISTERED: "DEREGISTERED",
            HealthStatus.REASON_OPTIONAL_DISABLED: "OPTIONAL_DISABLED",
            HealthStatus.REASON_MISSION_NOT_REQUIRED: "MISSION_NOT_REQUIRED",
        }.get(value, f"UNKNOWN_{value}")

    def management_reason_text(self, value):
        return {
            ManagementState.REASON_NONE: "NONE",
            ManagementState.REASON_MAINTENANCE_MODE: "MAINTENANCE_MODE",
            ManagementState.REASON_PLANNED_INACTIVE: "PLANNED_INACTIVE",
        }.get(value, f"UNKNOWN_{value}")

    def safety_state_text(self, value):
        return {
            SafetyStatus.SAFE: "SAFE",
            SafetyStatus.UNSAFE: "UNSAFE",
            SafetyStatus.DEGRADED: "DEGRADED",
            SafetyStatus.UNKNOWN: "UNKNOWN",
        }.get(value, f"UNKNOWN_{value}")

    def safety_reason_text(self, value):
        return {
            SafetyStatus.REASON_NONE: "NONE",
            SafetyStatus.REASON_INSUFFICIENT_BRAKING_DISTANCE: "INSUFFICIENT_BRAKING_DISTANCE",
            SafetyStatus.REASON_HEALTH_UNSAFE: "HEALTH_UNSAFE",
            SafetyStatus.REASON_INVALID_INPUT: "INVALID_INPUT",
            SafetyStatus.REASON_WAITING_FOR_INPUTS: "WAITING_FOR_INPUTS",
        }.get(value, f"UNKNOWN_{value}")

    def supervisor_mode_text(self, value):
        return {
            SupervisorStatus.NORMAL: "NORMAL",
            SupervisorStatus.HOLD: "HOLD",
            SupervisorStatus.FAILSAFE: "FAILSAFE",
            SupervisorStatus.EMERGENCY_STOP: "EMERGENCY_STOP",
            SupervisorStatus.UNKNOWN: "UNKNOWN",
        }.get(value, f"UNKNOWN_{value}")

    def supervisor_reason_text(self, value):
        return {
            SupervisorStatus.REASON_NONE: "NONE",
            SupervisorStatus.REASON_WAITING_FOR_SAFETY: "WAITING_FOR_SAFETY",
            SupervisorStatus.REASON_SAFETY_STATUS_STALE: "SAFETY_STATUS_STALE",
            SupervisorStatus.REASON_SAFETY_UNKNOWN: "SAFETY_UNKNOWN",
            SupervisorStatus.REASON_HEALTH_UNSAFE: "HEALTH_UNSAFE",
            SupervisorStatus.REASON_HEALTH_STATUS_STALE: "HEALTH_STATUS_STALE",
            SupervisorStatus.REASON_MAINTENANCE_MODE: "MAINTENANCE_MODE",
            SupervisorStatus.REASON_OBSTACLE_TOO_CLOSE: "OBSTACLE_TOO_CLOSE",
            SupervisorStatus.REASON_INVALID_SAFETY_INPUT: "INVALID_SAFETY_INPUT",
            SupervisorStatus.REASON_REQUIRED_HEALTH_FAILED: "REQUIRED_HEALTH_FAILED",
            SupervisorStatus.REASON_PLANNED_INACTIVE: "PLANNED_INACTIVE",
            SupervisorStatus.REASON_MANAGEMENT_STATUS_STALE: "MANAGEMENT_STATUS_STALE",
            SupervisorStatus.REASON_NETWORK_STATUS_STALE: "NETWORK_STATUS_STALE",
            SupervisorStatus.REASON_NETWORK_UNHEALTHY: "NETWORK_UNHEALTHY",

        }.get(value, f"UNKNOWN_{value}")

    def health_color(self, status):
        if status == HealthStatus.OK:
            return "green"
        if status in (HealthStatus.UNKNOWN, HealthStatus.WARNING):
            return "yellow"
        if status == HealthStatus.INACTIVE:
            return "gray"
        return "red"

    def safety_color(self, state):
        if state == SafetyStatus.SAFE:
            return "green"
        if state in (SafetyStatus.UNKNOWN, SafetyStatus.DEGRADED):
            return "yellow"
        return "red"

    def supervisor_color(self, mode):
        if mode == SupervisorStatus.NORMAL:
            return "green"
        if mode in (SupervisorStatus.HOLD, SupervisorStatus.UNKNOWN):
            return "yellow"
        return "red"


def main():
    rclpy.init()
    node = DashboardBridgeNode()

    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.httpd.shutdown()
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
