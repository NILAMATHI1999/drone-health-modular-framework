#include <array>
#include <chrono>
#include <cstdio>
#include <sstream>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/int32.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

class WifiMonitorNode : public rclcpp::Node
{
public:
  WifiMonitorNode() : Node("wifi_monitor_node")
  {
    state_pub_ = create_publisher<std_msgs::msg::String>("/network/wifi/state", 10);
    ssid_pub_ = create_publisher<std_msgs::msg::String>("/network/wifi/connected_ssid", 10);
    speed_pub_ = create_publisher<std_msgs::msg::Int32>("/network/wifi/link_speed_mbps", 10);
    signal_bars_pub_ = create_publisher<std_msgs::msg::Int32>("/network/wifi/signal_bars", 10);
    ssids_pub_ = create_publisher<std_msgs::msg::String>("/network/wifi/available_ssids", 10);
    auto heartbeat_qos = rclcpp::QoS(rclcpp::KeepLast(10));
    heartbeat_qos.reliable();
    heartbeat_qos.deadline(5000ms);
    heartbeat_qos.liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC);
    heartbeat_qos.liveliness_lease_duration(8000ms);
    heartbeat_pub_ = create_publisher<std_msgs::msg::String>("/network/wifi/heartbeat", heartbeat_qos);
    timer_ = create_wall_timer(1000ms, std::bind(&WifiMonitorNode::publish_wifi, this));
    RCLCPP_INFO(get_logger(), "WiFi monitor node started");
  }
private:
  void publish_wifi()
  {
    const std::string output = run_command("nmcli -t -f ACTIVE,SSID,RATE,SIGNAL,SECURITY dev wifi");
    std::stringstream stream(output);
    std::string line;
    bool first_ssid = true;
    std::string state = "DISCONNECTED";
    std::string connected_ssid = "--";
    std::string available_ssids;
    int speed_mbps = -1;
    int signal_bars = -1;
    while (std::getline(stream, line)) {
      if (line.empty()) continue;
      std::stringstream line_stream(line);
      std::string active, ssid, rate, signal, security;
      std::getline(line_stream, active, ':');
      std::getline(line_stream, ssid, ':');
      std::getline(line_stream, rate, ':');
      std::getline(line_stream, signal, ':');
      std::getline(line_stream, security);
      if (!ssid.empty()) {
        if (!first_ssid) available_ssids += ", ";
        available_ssids += ssid;
        first_ssid = false;
      }
      if (active == "yes") {
        state = "CONNECTED";
        connected_ssid = ssid.empty() ? "--" : ssid;
        try { speed_mbps = std::stoi(rate); } catch (const std::exception &) { speed_mbps = -1; }
        signal_bars = signal_percent_to_bars(signal);
      }
    }
    publish_string(state_pub_, state);
    publish_string(ssid_pub_, connected_ssid);
    publish_int(speed_pub_, speed_mbps);
    publish_int(signal_bars_pub_, signal_bars);
    publish_string(ssids_pub_, available_ssids);
    publish_string(heartbeat_pub_, "wifi_monitor_node alive");
    if (!heartbeat_pub_->assert_liveliness()) {
      RCLCPP_WARN(get_logger(), "Failed to assert network liveliness");
    }
  }
  int signal_percent_to_bars(const std::string & signal) const
  {
    int percent = -1;
    try { percent = std::stoi(signal); } catch (const std::exception &) { return -1; }
    if (percent >= 80) return 4;
    if (percent >= 60) return 3;
    if (percent >= 40) return 2;
    if (percent >= 20) return 1;
    if (percent >= 0) return 0;
    return -1;
  }

  std::string run_command(const std::string & command)
  {
    std::array<char, 256> buffer{};
    std::string result;
    FILE * pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) return result;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) result += buffer.data();
    pclose(pipe);
    return result;
  }
  void publish_string(const rclcpp::Publisher<std_msgs::msg::String>::SharedPtr & pub, const std::string & value)
  {
    std_msgs::msg::String msg; msg.data = value; pub->publish(msg);
  }
  void publish_int(const rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr & pub, int value)
  {
    std_msgs::msg::Int32 msg; msg.data = value; pub->publish(msg);
  }
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr state_pub_, ssid_pub_, ssids_pub_, heartbeat_pub_;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr speed_pub_, signal_bars_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};
int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<WifiMonitorNode>());
  rclcpp::shutdown();
  return 0;
}
