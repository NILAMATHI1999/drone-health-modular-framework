#!/usr/bin/env python3

import sys
import time

import rclpy
from rclpy.node import Node
from drone_health_interfaces.msg import HealthStatus


class HealthDetectionTimer(Node):
    def __init__(self, topic_name):
        super().__init__("health_detection_timer")
        self.topic_name = topic_name
        self.start_time = time.monotonic()

        self.create_subscription(
            HealthStatus,
            "/health/status",
            self.handle_health_status,
            10,
        )

        self.get_logger().info(
            f"Waiting for STALE status on {self.topic_name}. Kill the camera node now."
        )

    def handle_health_status(self, msg):
        if msg.node_name != "camera":
            return

        if msg.topic_name != self.topic_name:
            return

        if msg.status == HealthStatus.STALE:
            elapsed = time.monotonic() - self.start_time
            print("")
            print("Detection result")
            print(f"  topic: {msg.topic_name}")
            print("  status: STALE")
            print(f"  reason: {msg.reason}")
            print(f"  message: {msg.message}")
            print(f"  last_update_age_s: {msg.last_update_age_s:.3f}")
            print(f"  wall_time_to_detect_s: {elapsed:.3f}")
            rclpy.shutdown()


def main():
    topic_name = "/camera/heartbeat"

    if len(sys.argv) > 1:
        topic_name = sys.argv[1]

    rclpy.init()
    node = HealthDetectionTimer(topic_name)
    rclpy.spin(node)


if __name__ == "__main__":
    main()
