#include <chrono>
#include <fstream>
#include <functional>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>

#include "drone_health_interfaces/msg/monitor_spec.hpp"
#include "drone_health_interfaces/srv/deregister_module.hpp"
#include "drone_health_interfaces/srv/register_module.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/float32.hpp"
#include "std_msgs/msg/string.hpp"
#include "std_srvs/srv/trigger.hpp"

using namespace std::chrono_literals;

class CpuTemperaturePublisher : public rclcpp::Node
{
public:
  CpuTemperaturePublisher()
  : Node("cpu_temperature_publisher")
  {
    declare_parameters();
    read_parameters();
    setup_qos();
    setup_publishers();
    setup_clients();
    setup_services();
    setup_timer();

    RCLCPP_INFO(get_logger(), "CPU temperature publisher started");
  }

private:
  using RegisterModule = drone_health_interfaces::srv::RegisterModule;
  using DeregisterModule = drone_health_interfaces::srv::DeregisterModule;
  using MonitorSpec = drone_health_interfaces::msg::MonitorSpec;
  using Trigger = std_srvs::srv::Trigger;

  void declare_parameters()
  {
    declare_parameter<std::string>(
      "module_name",
      "cpu_temperature_publisher");

    declare_parameter<bool>(
      "critical",
      false);

    declare_parameter<std::string>(
      "thermal_file",
      "/sys/class/thermal/thermal_zone0/temp");

    declare_parameter<std::string>(
      "heartbeat_topic",
      "/system/cpu_temperature/heartbeat");

    declare_parameter<std::string>(
      "temperature_topic",
      "/system/cpu_temperature");

    declare_parameter<int>(
      "publish_period_ms",
      1000);

    declare_parameter<int>(
      "heartbeat_deadline_ms",
      1500);

    declare_parameter<int>(
      "heartbeat_liveliness_ms",
      0);

    declare_parameter<int>(
      "temperature_deadline_ms",
      2000);

    declare_parameter<int>(
      "auto_deregister_after_cycles",
      0);

    declare_parameter<std::string>(
      "request_deregister_service",
      "/cpu_temperature_publisher/request_deregister");
  }

  void read_parameters()
  {
    module_name_ =
      get_parameter("module_name").as_string();

    critical_ =
      get_parameter("critical").as_bool();

    thermal_file_ =
      get_parameter("thermal_file").as_string();

    heartbeat_topic_ =
      get_parameter("heartbeat_topic").as_string();

    temperature_topic_ =
      get_parameter("temperature_topic").as_string();

    publish_period_ms_ =
      get_parameter("publish_period_ms").as_int();

    heartbeat_deadline_ms_ =
      get_parameter("heartbeat_deadline_ms").as_int();

    heartbeat_liveliness_ms_ =
      get_parameter("heartbeat_liveliness_ms").as_int();

    temperature_deadline_ms_ =
      get_parameter("temperature_deadline_ms").as_int();

    auto_deregister_after_cycles_ =
      get_parameter("auto_deregister_after_cycles").as_int();

    request_deregister_service_name_ =
      get_parameter("request_deregister_service").as_string();

    if (module_name_.empty()) {
      throw std::runtime_error("module_name must not be empty");
    }

    if (thermal_file_.empty()) {
      throw std::runtime_error("thermal_file must not be empty");
    }

    if (heartbeat_topic_.empty() || heartbeat_topic_.front() != '/') {
      throw std::runtime_error("heartbeat_topic must start with /");
    }

    if (temperature_topic_.empty() || temperature_topic_.front() != '/') {
      throw std::runtime_error("temperature_topic must start with /");
    }

    if (
      request_deregister_service_name_.empty() ||
      request_deregister_service_name_.front() != '/')
    {
      throw std::runtime_error(
              "request_deregister_service must start with /");
    }

    if (publish_period_ms_ <= 0) {
      throw std::runtime_error(
              "publish_period_ms must be greater than 0");
    }

    if (heartbeat_deadline_ms_ < 0 || heartbeat_liveliness_ms_ < 0) {
      throw std::runtime_error(
              "heartbeat deadline/liveliness must not be negative");
    }

    if (heartbeat_deadline_ms_ == 0 && heartbeat_liveliness_ms_ == 0) {
      throw std::runtime_error(
              "heartbeat must use deadline or liveliness");
    }

    if (
      heartbeat_deadline_ms_ > 0 &&
      heartbeat_liveliness_ms_ > 0 &&
      heartbeat_liveliness_ms_ <= heartbeat_deadline_ms_)
    {
      throw std::runtime_error(
              "heartbeat_liveliness_ms must be greater than "
              "heartbeat_deadline_ms");
    }

    if (
      heartbeat_deadline_ms_ > 0 &&
      publish_period_ms_ >= heartbeat_deadline_ms_)
    {
      throw std::runtime_error(
              "publish_period_ms must be less than "
              "heartbeat_deadline_ms");
    }

    if (temperature_deadline_ms_ <= 0) {
      throw std::runtime_error(
              "temperature_deadline_ms must be greater than 0");
    }

    if (publish_period_ms_ >= temperature_deadline_ms_) {
      throw std::runtime_error(
              "publish_period_ms must be less than "
              "temperature_deadline_ms");
    }

    if (auto_deregister_after_cycles_ < 0) {
      throw std::runtime_error(
              "auto_deregister_after_cycles must not be negative");
    }
  }

  void setup_qos()
  {
    heartbeat_qos_ =
      rclcpp::QoS(rclcpp::KeepLast(10)).reliable();

    temperature_qos_ =
      rclcpp::QoS(rclcpp::KeepLast(10)).reliable();

    if (heartbeat_deadline_ms_ > 0) {
      heartbeat_qos_.deadline(
        std::chrono::milliseconds(heartbeat_deadline_ms_));
    }

    if (heartbeat_liveliness_ms_ > 0) {
      heartbeat_qos_
      .liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC)
      .liveliness_lease_duration(
        std::chrono::milliseconds(heartbeat_liveliness_ms_));
    }

    temperature_qos_.deadline(
      std::chrono::milliseconds(temperature_deadline_ms_));
  }

  void setup_publishers()
  {
    heartbeat_publisher_ =
      create_publisher<std_msgs::msg::String>(
      heartbeat_topic_,
      heartbeat_qos_);

    temperature_publisher_ =
      create_publisher<std_msgs::msg::Float32>(
      temperature_topic_,
      temperature_qos_);
  }

  void setup_clients()
  {
    register_client_ =
      create_client<RegisterModule>(
      "/management/register_module");

    deregister_client_ =
      create_client<DeregisterModule>(
      "/management/deregister_module");
  }

  void setup_services()
  {
    request_deregister_service_ =
      create_service<Trigger>(
      request_deregister_service_name_,
      std::bind(
        &CpuTemperaturePublisher::handle_request_deregister,
        this,
        std::placeholders::_1,
        std::placeholders::_2));
  }

  void setup_timer()
  {
    timer_ =
      create_wall_timer(
      std::chrono::milliseconds(publish_period_ms_),
      std::bind(
        &CpuTemperaturePublisher::timer_callback,
        this));
  }

  void timer_callback()
  {
    if (!registered_ && !register_request_pending_) {
      request_register();
    }

    if (
      deregister_requested_ &&
      !deregistered_ &&
      !deregister_request_pending_)
    {
      request_deregister("deregistered", true);
    }

    if (deregistered_) {
      if (shutdown_after_deregister_) {
        rclcpp::shutdown();
      }

      return;
    }

    if (!registered_) {
      return;
    }

    maybe_request_auto_deregister();

    if (deregister_requested_) {
      return;
    }

    publish_heartbeat();
    publish_temperature();
  }

  void request_register()
  {
    if (!register_client_->service_is_ready()) {
      RCLCPP_WARN_THROTTLE(
        get_logger(),
        *get_clock(),
        2000,
        "Management register service not ready; retrying");

      return;
    }

    auto request =
      std::make_shared<RegisterModule::Request>();

    request->module_name = module_name_;
    request->critical = critical_;

    MonitorSpec heartbeat_monitor;
    heartbeat_monitor.topic_name = heartbeat_topic_;
    heartbeat_monitor.kind = "heartbeat";
    heartbeat_monitor.message_type = "std_msgs/msg/String";
    heartbeat_monitor.reliability = "reliable";
    heartbeat_monitor.deadline_ms = heartbeat_deadline_ms_;
    heartbeat_monitor.liveliness_ms = heartbeat_liveliness_ms_;

    request->monitors.push_back(heartbeat_monitor);

    MonitorSpec temperature_monitor;
    temperature_monitor.topic_name = temperature_topic_;
    temperature_monitor.kind = "data";
    temperature_monitor.message_type = "std_msgs/msg/Float32";
    temperature_monitor.reliability = "reliable";
    temperature_monitor.deadline_ms = temperature_deadline_ms_;
    temperature_monitor.liveliness_ms = 0;

    request->monitors.push_back(temperature_monitor);

    register_request_pending_ = true;

    register_client_->async_send_request(
      request,
      [this](
        std::shared_future<RegisterModule::Response::SharedPtr> future)
      {
        register_request_pending_ = false;

        const auto response = future.get();

        if (!response->success) {
          RCLCPP_WARN(
            get_logger(),
            "Registration rejected: %s",
            response->message.c_str());

          return;
        }

        registered_ = true;

        RCLCPP_INFO(
          get_logger(),
          "Registration accepted: %s",
          response->message.c_str());
      });
  }

  void handle_request_deregister(
    const std::shared_ptr<Trigger::Request>,
    std::shared_ptr<Trigger::Response> response)
  {
    if (!registered_) {
      response->success = false;
      response->message = "module is not registered yet";
      return;
    }

    if (deregistered_) {
      response->success = true;
      response->message = "module is already deregistered";
      return;
    }

    deregister_requested_ = true;
    shutdown_after_deregister_ = true;

    response->success = true;
    response->message = "deregistration requested";
  }

  void request_deregister(
    const std::string & reason,
    bool shutdown_after_success)
  {
    if (!deregister_client_->service_is_ready()) {
      RCLCPP_WARN_THROTTLE(
        get_logger(),
        *get_clock(),
        2000,
        "Management deregister service not ready; retrying");

      return;
    }

    auto request =
      std::make_shared<DeregisterModule::Request>();

    request->module_name = module_name_;
    request->reason = reason;

    deregister_request_pending_ = true;

    deregister_client_->async_send_request(
      request,
      [this, shutdown_after_success](
        std::shared_future<DeregisterModule::Response::SharedPtr> future)
      {
        deregister_request_pending_ = false;

        const auto response = future.get();

        if (!response->success) {
          RCLCPP_WARN(
            get_logger(),
            "Deregistration rejected: %s",
            response->message.c_str());

          return;
        }

        deregistered_ = true;
        deregister_requested_ = false;
        shutdown_after_deregister_ = shutdown_after_success;

        RCLCPP_INFO(
          get_logger(),
          "Deregistration accepted: %s",
          response->message.c_str());
      });
  }

  void maybe_request_auto_deregister()
  {
    if (auto_deregister_after_cycles_ == 0) {
      return;
    }

    if (
      deregister_requested_ ||
      deregister_request_pending_ ||
      deregistered_)
    {
      return;
    }

    ++active_cycle_count_;

    if (active_cycle_count_ < auto_deregister_after_cycles_) {
      return;
    }

    RCLCPP_INFO(
      get_logger(),
      "Auto deregistration condition reached after %d cycles",
      active_cycle_count_);

    deregister_requested_ = true;
    shutdown_after_deregister_ = true;
  }

  void publish_heartbeat()
  {
    std_msgs::msg::String heartbeat;
    heartbeat.data = module_name_ + " alive";

    heartbeat_publisher_->publish(heartbeat);

    if (heartbeat_liveliness_ms_ > 0) {
      heartbeat_publisher_->assert_liveliness();
    }
  }

  void publish_temperature()
  {
    std::ifstream file(thermal_file_);

    if (!file.is_open()) {
      RCLCPP_WARN(
        get_logger(),
        "Cannot open thermal file: %s",
        thermal_file_.c_str());

      return;
    }

    float temperature_millidegrees = 0.0F;
    file >> temperature_millidegrees;

    if (file.fail()) {
      RCLCPP_WARN(
        get_logger(),
        "Failed to read CPU temperature");

      return;
    }

    std_msgs::msg::Float32 message;
    message.data = temperature_millidegrees / 1000.0F;

    temperature_publisher_->publish(message);

    RCLCPP_INFO(
      get_logger(),
      "CPU temperature: %.2f deg C",
      message.data);
  }

  std::string module_name_;
  bool critical_{false};

  std::string thermal_file_;
  std::string heartbeat_topic_;
  std::string temperature_topic_;
  std::string request_deregister_service_name_;

  int publish_period_ms_{1000};
  int heartbeat_deadline_ms_{1500};
  int heartbeat_liveliness_ms_{0};
  int temperature_deadline_ms_{2000};
  int auto_deregister_after_cycles_{0};
  int active_cycle_count_{0};

  bool registered_{false};
  bool register_request_pending_{false};
  bool deregister_requested_{false};
  bool deregister_request_pending_{false};
  bool deregistered_{false};
  bool shutdown_after_deregister_{false};

  rclcpp::QoS heartbeat_qos_{
    rclcpp::KeepLast(10)};

  rclcpp::QoS temperature_qos_{
    rclcpp::KeepLast(10)};

  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr
    heartbeat_publisher_;

  rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr
    temperature_publisher_;

  rclcpp::Client<RegisterModule>::SharedPtr
    register_client_;

  rclcpp::Client<DeregisterModule>::SharedPtr
    deregister_client_;

  rclcpp::Service<Trigger>::SharedPtr
    request_deregister_service_;

  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(
    std::make_shared<CpuTemperaturePublisher>());
  rclcpp::shutdown();
  return 0;
}
