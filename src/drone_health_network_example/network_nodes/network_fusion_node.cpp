#include <chrono>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

class NetworkFusionNode : public rclcpp::Node
{
public:
  NetworkFusionNode() : Node("network_fusion_node")
  {
    status_pub_ = create_publisher<std_msgs::msg::String>("/network_status", 10);
    reason_pub_ = create_publisher<std_msgs::msg::String>("/network_reason", 10);
    auto heartbeat_qos = rclcpp::QoS(rclcpp::KeepLast(10));
    heartbeat_qos.reliable();
    heartbeat_qos.deadline(5000ms);
    heartbeat_qos.liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC);
    heartbeat_qos.liveliness_lease_duration(8000ms);
    heartbeat_pub_ = create_publisher<std_msgs::msg::String>("/network/heartbeat", heartbeat_qos);
    wifi_sub_ = create_subscription<std_msgs::msg::String>("/network/wifi/state", 10, [this](std_msgs::msg::String::SharedPtr msg) { wifi_state_ = msg->data; wifi_seen_ = now(); });
    lte_sub_ = create_subscription<std_msgs::msg::String>("/network/lte/state", 10, [this](std_msgs::msg::String::SharedPtr msg) { lte_state_ = msg->data; lte_seen_ = now(); });
    timer_ = create_wall_timer(400ms, std::bind(&NetworkFusionNode::publish_status, this));
    RCLCPP_INFO(get_logger(), "Network fusion node started");
  }
private:
  void publish_status()
  {
    const bool wifi_fresh = fresh(wifi_seen_, 10.0);
    const bool lte_fresh = fresh(lte_seen_, 10.0);
    const bool wifi_connected = wifi_fresh && wifi_state_ == "CONNECTED";
    const bool lte_connected = lte_fresh && lte_state_ == "CONNECTED";
    std::string status, active;
    if (wifi_connected) { status = "NETWORK_HEALTHY"; active = "WIFI"; }
    else if (lte_connected) { status = "NETWORK_BACKUP"; active = "LTE"; }
    else { status = "NETWORK_UNHEALTHY"; active = "NONE"; }
    publish_string(status_pub_, status);
    publish_string(reason_pub_, "WIFI=" + link_text(wifi_fresh, wifi_state_) + ",LTE=" + link_text(lte_fresh, lte_state_) + ",ACTIVE_CONNECTION=" + active);
    publish_string(heartbeat_pub_, "network_fusion_node alive");
    if (!heartbeat_pub_->assert_liveliness()) {
      RCLCPP_WARN(get_logger(), "Failed to assert network liveliness");
    }
  }
  bool fresh(const rclcpp::Time & seen, double timeout_s) const { return seen.nanoseconds() != 0 && (now() - seen).seconds() <= timeout_s; }
  std::string link_text(bool fresh_state, const std::string & state) const { return fresh_state ? state : "STALE"; }
  void publish_string(const rclcpp::Publisher<std_msgs::msg::String>::SharedPtr & pub, const std::string & value)
  {
    std_msgs::msg::String msg; msg.data = value; pub->publish(msg);
  }
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr status_pub_, reason_pub_, heartbeat_pub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr wifi_sub_, lte_sub_;
  rclcpp::TimerBase::SharedPtr timer_;
  std::string wifi_state_{"DISCONNECTED"};
  std::string lte_state_{"DISCONNECTED"};
  rclcpp::Time wifi_seen_{0, 0, RCL_ROS_TIME};
  rclcpp::Time lte_seen_{0, 0, RCL_ROS_TIME};
};
int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<NetworkFusionNode>());
  rclcpp::shutdown();
  return 0;
}
