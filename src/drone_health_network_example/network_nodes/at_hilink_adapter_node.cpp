#include <array>
#include <chrono>
#include <cstdio>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

class AtHilinkAdapterNode : public rclcpp::Node
{
public:
  AtHilinkAdapterNode() : Node("at_hilink_adapter_node")
  {
    state_pub_ = create_publisher<std_msgs::msg::String>("/network/at_hilink/state", 10);
    operator_pub_ = create_publisher<std_msgs::msg::String>("/network/at_hilink/operator", 10);
    rat_pub_ = create_publisher<std_msgs::msg::String>("/network/at_hilink/rat", 10);
    rssi_pub_ = create_publisher<std_msgs::msg::String>("/network/at_hilink/rssi_dbm", 10);
    rsrp_pub_ = create_publisher<std_msgs::msg::String>("/network/at_hilink/rsrp_dbm", 10);
    rsrq_pub_ = create_publisher<std_msgs::msg::String>("/network/at_hilink/rsrq_db", 10);
    sinr_pub_ = create_publisher<std_msgs::msg::String>("/network/at_hilink/sinr_db", 10);
    plmn_pub_ = create_publisher<std_msgs::msg::String>("/network/at_hilink/plmn", 10);
    at_summary_pub_ = create_publisher<std_msgs::msg::String>("/network/at_hilink/at_summary", 10);

    auto heartbeat_qos = rclcpp::QoS(rclcpp::KeepLast(10));
    heartbeat_qos.reliable();
    heartbeat_qos.deadline(2500ms);
    heartbeat_qos.liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC);
    heartbeat_qos.liveliness_lease_duration(5000ms);
    heartbeat_pub_ = create_publisher<std_msgs::msg::String>("/network/at_hilink/heartbeat", heartbeat_qos);

    timer_ = create_wall_timer(2000ms, std::bind(&AtHilinkAdapterNode::poll_hilink, this));
    RCLCPP_INFO(get_logger(), "AT-over-HiLink adapter node started");
  }

private:
  void poll_hilink()
  {
    const std::string signal = run_command(
      "curl --connect-timeout 0.3 --max-time 0.8 -s http://192.168.8.1/api/device/signal");
    const std::string plmn = run_command(
      "curl --connect-timeout 0.3 --max-time 0.8 -s http://192.168.8.1/api/net/current-plmn");

    const std::string operator_name = dash(xml_value(plmn, "FullName"));
    const std::string rat = dash(rat_text(xml_value(plmn, "Rat")));
    const std::string rssi = dash(xml_value(signal, "rssi"));
    const std::string rsrp = dash(xml_value(signal, "rsrp"));
    const std::string rsrq = dash(xml_value(signal, "rsrq"));
    const std::string sinr = dash(xml_value(signal, "sinr"));
    const std::string plmn_code = dash(xml_value(plmn, "Numeric"));
    const bool reachable = !signal.empty() || !plmn.empty();

    publish_string(state_pub_, reachable ? "OK_FROM_AT" : "NO_RESPONSE_FROM_AT");
    publish_string(operator_pub_, "AT+COPS? -> " + operator_name);
    publish_string(rat_pub_, "AT^SYSINFOEX -> " + rat);
    publish_string(rssi_pub_, "AT+CSQ -> " + rssi);
    publish_string(rsrp_pub_, "AT^HCSQ? -> " + rsrp);
    publish_string(rsrq_pub_, "AT^HCSQ? -> " + rsrq);
    publish_string(sinr_pub_, "AT^HCSQ? -> " + sinr);
    publish_string(plmn_pub_, "AT+COPS? -> " + plmn_code);
    publish_string(
      at_summary_pub_,
      "AT=OK; AT+CSQ=" + rssi + "; AT+COPS?=" + operator_name +
      "; AT^SYSINFOEX=" + rat + "; AT^HCSQ?=" + rsrp + "," + rsrq + "," + sinr);
    publish_string(heartbeat_pub_, "at_hilink_adapter_node alive");
    if (!heartbeat_pub_->assert_liveliness()) {
      RCLCPP_WARN(get_logger(), "Failed to assert AT-over-HiLink liveliness");
    }
  }

  std::string run_command(const std::string & command)
  {
    std::array<char, 256> buffer{};
    std::string result;
    FILE * pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) {
      return result;
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
      result += buffer.data();
    }
    pclose(pipe);
    return result;
  }

  std::string xml_value(const std::string & xml, const std::string & tag) const
  {
    const std::string open = "<" + tag + ">";
    const std::string close = "</" + tag + ">";
    const auto start = xml.find(open);
    if (start == std::string::npos) return "";
    const auto value_start = start + open.size();
    const auto end = xml.find(close, value_start);
    if (end == std::string::npos) return "";
    return xml.substr(value_start, end - value_start);
  }

  std::string rat_text(const std::string & rat) const
  {
    if (rat == "7") return "LTE/4G";
    if (rat.empty()) return "";
    return "RAT " + rat;
  }

  std::string dash(const std::string & value) const
  {
    return value.empty() ? "--" : value;
  }

  void publish_string(
    const rclcpp::Publisher<std_msgs::msg::String>::SharedPtr & pub,
    const std::string & value)
  {
    std_msgs::msg::String msg;
    msg.data = value;
    pub->publish(msg);
  }

  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr state_pub_, operator_pub_, rat_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr rssi_pub_, rsrp_pub_, rsrq_pub_, sinr_pub_, plmn_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr at_summary_pub_, heartbeat_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<AtHilinkAdapterNode>());
  rclcpp::shutdown();
  return 0;
}
