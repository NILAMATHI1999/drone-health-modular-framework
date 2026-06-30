#include <chrono>
#include <stdexcept>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

class AtModemMonitorNode : public rclcpp::Node
{
public:
  AtModemMonitorNode() : Node("at_modem_monitor_node")
  {
    declare_parameters();
    read_parameters();
    validate_parameters();
    setup_publishers();
    setup_timer();

    RCLCPP_INFO(
      get_logger(),
      "AT modem monitor started in %s mode on %s at %d baud",
      mock_mode_ ? "mock" : "serial-template",
      serial_port_.c_str(),
      baud_rate_);
  }

private:
  void declare_parameters()
  {
    declare_parameter<bool>("mock_mode", true);
    declare_parameter<std::string>("serial_port", "/dev/ttyUSB0");
    declare_parameter<int>("baud_rate", 115200);
    declare_parameter<int>("poll_period_ms", 2000);
    declare_parameter<int>("command_delay_ms", 2000);
    declare_parameter<int>("response_timeout_ms", 1000);
  }

  void read_parameters()
  {
    mock_mode_ = get_parameter("mock_mode").as_bool();
    serial_port_ = get_parameter("serial_port").as_string();
    baud_rate_ = get_parameter("baud_rate").as_int();
    poll_period_ms_ = get_parameter("poll_period_ms").as_int();
    command_delay_ms_ = get_parameter("command_delay_ms").as_int();
    response_timeout_ms_ = get_parameter("response_timeout_ms").as_int();
  }

  void validate_parameters() const
  {
    if (serial_port_.empty()) {
      throw std::runtime_error("serial_port must not be empty");
    }

    if (baud_rate_ <= 0 ||
      poll_period_ms_ <= 0 ||
      command_delay_ms_ <= 0 ||
      response_timeout_ms_ <= 0)
    {
      throw std::runtime_error("AT modem timing and baud parameters must be greater than 0");
    }
  }

  void setup_publishers()
  {
    state_pub_ = create_publisher<std_msgs::msg::String>("/network/at_lte/state", 10);
    operator_pub_ = create_publisher<std_msgs::msg::String>("/network/at_lte/operator", 10);
    rat_pub_ = create_publisher<std_msgs::msg::String>("/network/at_lte/rat", 10);
    rssi_pub_ = create_publisher<std_msgs::msg::String>("/network/at_lte/rssi_dbm", 10);
    rsrp_pub_ = create_publisher<std_msgs::msg::String>("/network/at_lte/rsrp_dbm", 10);
    rsrq_pub_ = create_publisher<std_msgs::msg::String>("/network/at_lte/rsrq_db", 10);
    sinr_pub_ = create_publisher<std_msgs::msg::String>("/network/at_lte/sinr_db", 10);
    plmn_pub_ = create_publisher<std_msgs::msg::String>("/network/at_lte/plmn", 10);

    auto heartbeat_qos = rclcpp::QoS(rclcpp::KeepLast(10));
    heartbeat_qos.reliable();
    heartbeat_qos.deadline(std::chrono::milliseconds(2500));
    heartbeat_qos.liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC);
    heartbeat_qos.liveliness_lease_duration(std::chrono::milliseconds(5000));
    heartbeat_pub_ = create_publisher<std_msgs::msg::String>(
      "/network/at_lte/heartbeat", heartbeat_qos);
  }

  void setup_timer()
  {
    timer_ = create_wall_timer(
      std::chrono::milliseconds(poll_period_ms_),
      std::bind(&AtModemMonitorNode::poll_modem, this));
  }

  void poll_modem()
  {
    if (mock_mode_) {
      publish_mock_values();
      return;
    }

    publish_serial_template_unavailable();
  }

  void publish_mock_values()
  {
    publish_string(state_pub_, "CONNECTED_MOCK");
    publish_string(operator_pub_, "MOCK_OPERATOR from AT+COPS?");
    publish_string(rat_pub_, "LTE/4G from AT^SYSINFOEX");
    publish_string(rssi_pub_, "-65 dBm from AT+CSQ");
    publish_string(rsrp_pub_, "-96 dBm from AT^HCSQ?");
    publish_string(rsrq_pub_, "-12 dB from AT^HCSQ?");
    publish_string(sinr_pub_, "2 dB from AT^HCSQ?");
    publish_string(plmn_pub_, "26202 from AT+COPS?");
    publish_heartbeat();
  }

  void publish_serial_template_unavailable()
  {
    RCLCPP_WARN_THROTTLE(
      get_logger(), *get_clock(), 5000,
      "Serial AT backend is a template only. Future users should implement "
      "send_at_command() for %s at %d baud, with command_delay_ms=%d and "
      "response_timeout_ms=%d.",
      serial_port_.c_str(),
      baud_rate_,
      command_delay_ms_,
      response_timeout_ms_);

    // Future serial implementation flow:
    // AT          -> modem availability, expect OK
    // AT+CSQ     -> RSSI/quality, convert CSQ to dBm where supported
    // AT+COPS?   -> operator and PLMN
    // AT^SYSINFOEX or modem-specific equivalent -> RAT/technology
    // AT^HCSQ? or modem-specific equivalent -> RSRP/RSRQ/SINR
    (void)send_at_command("AT");

    publish_disconnected();
  }

  std::string send_at_command(const std::string & command)
  {
    (void)command;

    // Template extension point for future students with serial AT hardware:
    // 1. Open serial_port_ using baud_rate_ and raw terminal settings.
    // 2. Write command + "\r".
    // 3. Wait command_delay_ms_ between modem requests.
    // 4. Read until OK, ERROR, or response_timeout_ms_ expires.
    // 5. Return the raw response for parser functions.
    return "";
  }

  void publish_disconnected()
  {
    publish_string(state_pub_, "DISCONNECTED");
    publish_string(operator_pub_, "--");
    publish_string(rat_pub_, "--");
    publish_string(rssi_pub_, "--");
    publish_string(rsrp_pub_, "--");
    publish_string(rsrq_pub_, "--");
    publish_string(sinr_pub_, "--");
    publish_string(plmn_pub_, "--");
    publish_heartbeat();
  }

  void publish_heartbeat()
  {
    publish_string(heartbeat_pub_, "at_modem_monitor_node alive");
    if (!heartbeat_pub_->assert_liveliness()) {
      RCLCPP_WARN(get_logger(), "Failed to assert AT modem liveliness");
    }
  }

  void publish_string(
    const rclcpp::Publisher<std_msgs::msg::String>::SharedPtr & pub,
    const std::string & value)
  {
    std_msgs::msg::String msg;
    msg.data = value;
    pub->publish(msg);
  }

  bool mock_mode_{true};
  std::string serial_port_;
  int baud_rate_{115200};
  int poll_period_ms_{2000};
  int command_delay_ms_{2000};
  int response_timeout_ms_{1000};

  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr state_pub_, operator_pub_, rat_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr rssi_pub_, rsrp_pub_, rsrq_pub_, sinr_pub_, plmn_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr heartbeat_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<AtModemMonitorNode>());
  rclcpp::shutdown();
  return 0;
}
