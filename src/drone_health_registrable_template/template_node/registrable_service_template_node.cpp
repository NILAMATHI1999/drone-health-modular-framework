  #include <chrono>
  #include <future>
  #include <memory>
  #include <stdexcept>
  #include <string>

  #include "drone_health_interfaces/msg/monitor_spec.hpp"
  #include "drone_health_interfaces/srv/deregister_module.hpp"
  #include "drone_health_interfaces/srv/register_module.hpp"
  #include "rclcpp/rclcpp.hpp"
  #include "std_msgs/msg/string.hpp"
  #include "std_srvs/srv/trigger.hpp"


// Reusable service template.
// Future user should replace:
// - /template_service/run_action with the real service name.
// - Trigger service type with the real service type if needed.
// - handle_run_action() with real command/action logic.
// - default topic names in declare_parameters() or YAML config.
// Keep:
// - register_module request with MonitorSpec[]
// - heartbeat publisher
// - request_deregister flow


class RegistrableServiceTemplateNode : public rclcpp::Node
{
public:
  RegistrableServiceTemplateNode()
  : Node("registrable_service_template_node")
  {
    declare_parameters();
    read_parameters();
    setup_qos();
    setup_communication();

    RCLCPP_INFO(get_logger(), "Registrable service template node started");
  }

private:
  using DeregisterModule = drone_health_interfaces::srv::DeregisterModule;
  using MonitorSpec = drone_health_interfaces::msg::MonitorSpec;
  using RegisterModule = drone_health_interfaces::srv::RegisterModule;
  using Trigger = std_srvs::srv::Trigger;

  void declare_parameters()
  {
    declare_parameter<std::string>("module_name", "service_template_node");
    declare_parameter<bool>("critical", false);
    declare_parameter<std::string>("heartbeat_topic", "/template_service/heartbeat");
    declare_parameter<int>("heartbeat_period_ms", 200);
    declare_parameter<int>("heartbeat_deadline_ms", 500);
    declare_parameter<int>("heartbeat_liveliness_ms", 0);
    declare_parameter<int>("auto_deregister_after_cycles", 0);
    declare_parameter<std::string>(
        "request_deregister_service",
        "/template_service/request_deregister");


  }

  void read_parameters()
  {
    module_name_ = get_parameter("module_name").as_string();
    critical_ = get_parameter("critical").as_bool();
    heartbeat_topic_ = get_parameter("heartbeat_topic").as_string();
    heartbeat_period_ms_ = get_parameter("heartbeat_period_ms").as_int();
    heartbeat_deadline_ms_ = get_parameter("heartbeat_deadline_ms").as_int();
    heartbeat_liveliness_ms_ = get_parameter("heartbeat_liveliness_ms").as_int();

    auto_deregister_after_cycles_ =
      get_parameter("auto_deregister_after_cycles").as_int();
    request_deregister_service_name_ =
      get_parameter("request_deregister_service").as_string();

    if (request_deregister_service_name_.empty() ||
      request_deregister_service_name_.front() != '/')
    {
      throw std::runtime_error("request_deregister_service must start with /");
    }


    if (auto_deregister_after_cycles_ < 0) {
      throw std::runtime_error("auto_deregister_after_cycles must not be negative");
    }


    if (module_name_.empty() || heartbeat_topic_.empty()) {
      throw std::runtime_error("module_name and heartbeat_topic must not be empty");
    }

    if (heartbeat_topic_.front() != '/') {
      throw std::runtime_error("heartbeat_topic must start with /");
    }

    if (heartbeat_period_ms_ <= 0 || heartbeat_deadline_ms_ <= heartbeat_period_ms_) {
      throw std::runtime_error("heartbeat timing must satisfy period < deadline");
    }

    if (heartbeat_liveliness_ms_ > 0 &&
      heartbeat_liveliness_ms_ <= heartbeat_deadline_ms_)
    {
      throw std::runtime_error("heartbeat_liveliness_ms must be greater than deadline");
    }
  }

  void setup_qos()
  {
    heartbeat_qos_ = rclcpp::QoS(rclcpp::KeepLast(10)).reliable()
      .deadline(std::chrono::milliseconds(heartbeat_deadline_ms_));

    if (heartbeat_liveliness_ms_ > 0) {
      heartbeat_qos_
      .liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC)
      .liveliness_lease_duration(std::chrono::milliseconds(heartbeat_liveliness_ms_));
    }
  }


    // Optional internal self-deregistration example.
    // Default 0 disables it.
    // Future users can replace this cycle-count condition with real task logic:
    // task complete, calibration complete, mission phase finished, or payload no longer required.


  void maybe_request_auto_deregister()
  {
    if (auto_deregister_after_cycles_ == 0) {
      return;
    }

    if (deregister_requested_ || deregister_request_pending_ || deregistered_) {
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


  void setup_communication()
  {
    example_service_ = create_service<Trigger>(
        "/template_service/run_action",
        std::bind(
          &RegistrableServiceTemplateNode::handle_run_action,
          this,
          std::placeholders::_1,
          std::placeholders::_2));

    heartbeat_publisher_ = create_publisher<std_msgs::msg::String>(
        heartbeat_topic_,
        heartbeat_qos_);

    register_client_ = create_client<RegisterModule>("/management/register_module");
    deregister_client_ = create_client<DeregisterModule>("/management/deregister_module");

    request_deregister_service_ = create_service<Trigger>(
        request_deregister_service_name_,
        std::bind(
          &RegistrableServiceTemplateNode::handle_request_deregister,
          this,
          std::placeholders::_1,
          std::placeholders::_2));

    timer_ = create_wall_timer(
        std::chrono::milliseconds(heartbeat_period_ms_),
        std::bind(&RegistrableServiceTemplateNode::timer_callback, this));
  }

  MonitorSpec make_monitor(
    const std::string & topic_name,
    const std::string & kind,
    const std::string & message_type,
    const std::string & reliability,
    int deadline_ms,
    int liveliness_ms) const
  {
    MonitorSpec monitor;
    monitor.topic_name = topic_name;
    monitor.kind = kind;
    monitor.message_type = message_type;
    monitor.reliability = reliability;
    monitor.deadline_ms = deadline_ms;
    monitor.liveliness_ms = liveliness_ms;
    return monitor;
  }


  void timer_callback()
  {
    if (!registered_ && !register_request_pending_) {
      request_register();
    }

    if (deregister_requested_ && !deregistered_ && !deregister_request_pending_) {
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

    auto request = std::make_shared<RegisterModule::Request>();
    request->module_name = module_name_;
    request->critical = critical_;

    request->monitors.push_back(
        make_monitor(
          heartbeat_topic_,
          "heartbeat",
          "std_msgs/msg/String",
          "reliable",
          heartbeat_deadline_ms_,
          heartbeat_liveliness_ms_));

    register_request_pending_ = true;

    register_client_->async_send_request(
        request,
      [this](std::shared_future<RegisterModule::Response::SharedPtr> future)
      {
        register_request_pending_ = false;
        const auto response = future.get();

        if (!response->success) {
          RCLCPP_WARN(get_logger(), "Registration rejected: %s", response->message.c_str());
          return;
        }

        registered_ = true;
        RCLCPP_INFO(get_logger(), "Registration accepted: %s", response->message.c_str());
        });
  }

  void request_deregister(const std::string & reason, bool shutdown_after_success)
  {
    if (!deregister_client_->service_is_ready()) {
      RCLCPP_WARN_THROTTLE(
          get_logger(),
          *get_clock(),
          2000,
          "Management deregister service not ready; retrying");
      return;
    }

    auto request = std::make_shared<DeregisterModule::Request>();
    request->module_name = module_name_;
    request->reason = reason;

    deregister_request_pending_ = true;

    deregister_client_->async_send_request(
        request,
      [this,
      shutdown_after_success](std::shared_future<DeregisterModule::Response::SharedPtr> future)
      {
        deregister_request_pending_ = false;
        const auto response = future.get();

        if (!response->success) {
          RCLCPP_WARN(get_logger(), "Deregistration rejected: %s", response->message.c_str());
          return;
        }

        deregistered_ = true;
        deregister_requested_ = false;
        shutdown_after_deregister_ = shutdown_after_success;
        RCLCPP_INFO(get_logger(), "Deregistration accepted: %s", response->message.c_str());
        });
  }

// Replace this service callback with real request/response logic.
// Example: reset sensor, calibrate device, take picture, save map, or change mode.

  void handle_run_action(
    const std::shared_ptr<Trigger::Request>,
    std::shared_ptr<Trigger::Response> response)
  {
    ++service_call_count_;
    response->success = true;
    response->message = "example service action completed";
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

    deregister_requested_ = true;
    shutdown_after_deregister_ = true;

    response->success = true;
    response->message = "deregistration requested";
  }

  void publish_heartbeat()
  {
    std_msgs::msg::String heartbeat;
    heartbeat.data =
      module_name_ + " alive service_calls=" + std::to_string(service_call_count_);
    heartbeat_publisher_->publish(heartbeat);

    if (heartbeat_liveliness_ms_ > 0) {
      heartbeat_publisher_->assert_liveliness();
    }
  }

  std::string module_name_;
  bool critical_{false};
  std::string heartbeat_topic_;
  std::string request_deregister_service_name_;
  int heartbeat_period_ms_;
  int heartbeat_deadline_ms_;
  int heartbeat_liveliness_ms_;
  int service_call_count_{0};


  int auto_deregister_after_cycles_{0};
  int active_cycle_count_{0};


  bool registered_{false};
  bool register_request_pending_{false};
  bool deregister_requested_{false};
  bool deregister_request_pending_{false};
  bool deregistered_{false};
  bool shutdown_after_deregister_{false};

  rclcpp::QoS heartbeat_qos_{rclcpp::KeepLast(10)};
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr heartbeat_publisher_;
  rclcpp::Client<RegisterModule>::SharedPtr register_client_;
  rclcpp::Client<DeregisterModule>::SharedPtr deregister_client_;
  rclcpp::Service<Trigger>::SharedPtr example_service_;
  rclcpp::Service<Trigger>::SharedPtr request_deregister_service_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<RegistrableServiceTemplateNode>());
  rclcpp::shutdown();
  return 0;
}
