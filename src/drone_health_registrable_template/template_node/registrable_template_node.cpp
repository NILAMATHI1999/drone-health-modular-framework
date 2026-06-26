#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "drone_health_interfaces/srv/deregister_module.hpp"
#include "drone_health_interfaces/srv/register_module.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "std_srvs/srv/trigger.hpp"

  using namespace std::chrono_literals;

  class RegistrableTemplateNode : public rclcpp::Node
  {
  public:
    RegistrableTemplateNode()
    : Node("registrable_template_node")
    {
      declare_parameters();
      read_parameters();
      setup_qos();
      setup_publishers();
      setup_clients();
      setup_services();
      setup_timer();

      RCLCPP_INFO(get_logger(), "Registrable template node started");
    }

  private:
    using RegisterModule = drone_health_interfaces::srv::RegisterModule;
    using DeregisterModule = drone_health_interfaces::srv::DeregisterModule;
    using Trigger = std_srvs::srv::Trigger;

    void declare_parameters()
    {
      declare_parameter<std::string>("module_name", "template_node");
      declare_parameter<bool>("critical", false);
      declare_parameter<std::string>("heartbeat_topic", "/template/heartbeat");
      declare_parameter<int>("publish_period_ms", 200);
      declare_parameter<int>("heartbeat_deadline_ms", 500);
      declare_parameter<int>("heartbeat_liveliness_ms", 0);
    }

    void read_parameters()
    {
      module_name_ = get_parameter("module_name").as_string();
      critical_ = get_parameter("critical").as_bool();
      heartbeat_topic_ = get_parameter("heartbeat_topic").as_string();
      publish_period_ms_ = get_parameter("publish_period_ms").as_int();
      heartbeat_deadline_ms_ = get_parameter("heartbeat_deadline_ms").as_int();
      heartbeat_liveliness_ms_ = get_parameter("heartbeat_liveliness_ms").as_int();

      if (module_name_.empty() || heartbeat_topic_.empty()) {
        throw std::runtime_error("module_name and heartbeat_topic must not be empty");
      }

      if (publish_period_ms_ <= 0) {
        throw std::runtime_error("publish_period_ms must be greater than 0");
      }

      if (heartbeat_deadline_ms_ < 0 || heartbeat_liveliness_ms_ < 0) {
        throw std::runtime_error("heartbeat deadline/liveliness must not be negative");
      }

      if (heartbeat_deadline_ms_ == 0 && heartbeat_liveliness_ms_ == 0) {
        throw std::runtime_error("heartbeat must use deadline or liveliness");
      }

      if (heartbeat_deadline_ms_ > 0 &&
          heartbeat_liveliness_ms_ > 0 &&
          heartbeat_liveliness_ms_ <= heartbeat_deadline_ms_)
      {
        throw std::runtime_error(
          "heartbeat_liveliness_ms must be greater than heartbeat_deadline_ms");
      }

      if (heartbeat_deadline_ms_ > 0 && publish_period_ms_ >= heartbeat_deadline_ms_) {
        throw std::runtime_error("publish_period_ms must be less than heartbeat_deadline_ms");
      }
    }

    void setup_qos()
    {
      heartbeat_qos_ = rclcpp::QoS(rclcpp::KeepLast(10)).reliable();

      if (heartbeat_deadline_ms_ > 0) {
        heartbeat_qos_.deadline(std::chrono::milliseconds(heartbeat_deadline_ms_));
      }

      if (heartbeat_liveliness_ms_ > 0) {
        heartbeat_qos_
          .liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC)
          .liveliness_lease_duration(std::chrono::milliseconds(heartbeat_liveliness_ms_)
          );
      }
    }

    void setup_publishers()
    {
      heartbeat_publisher_ = create_publisher<std_msgs::msg::String>(
        heartbeat_topic_,
        heartbeat_qos_);
    }

    void setup_clients()
    {
      register_client_ = create_client<RegisterModule>("/management/register_module");
      deregister_client_ = create_client<DeregisterModule>("/management/deregister_module");
    }

    void setup_services()
    {
      request_deregister_service_ = create_service<Trigger>(
        "/template/request_deregister",
        std::bind(
          &RegistrableTemplateNode::handle_request_deregister,
          this,
          std::placeholders::_1,
          std::placeholders::_2));
    }

    void setup_timer()
    {
      timer_ = create_wall_timer(
        std::chrono::milliseconds(publish_period_ms_),
        std::bind(&RegistrableTemplateNode::timer_callback, this));
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
      request->heartbeat_topic = heartbeat_topic_;
      request->heartbeat_type = "std_msgs/msg/String";
      request->heartbeat_deadline_ms = heartbeat_deadline_ms_;
      request->heartbeat_liveliness_ms = heartbeat_liveliness_ms_;
      request->data_topics = {};
      request->data_topic_types = {};

      register_request_pending_ = true;

      register_client_->async_send_request(
        request,
        [this](std::shared_future<RegisterModule::Response::SharedPtr> future)
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
        [this, shutdown_after_success]
        (std::shared_future<DeregisterModule::Response::SharedPtr> future)
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

    void publish_heartbeat()
    {
      std_msgs::msg::String heartbeat;
      heartbeat.data = module_name_ + " alive";
      heartbeat_publisher_->publish(heartbeat);

      if (heartbeat_liveliness_ms_ > 0) {
        heartbeat_publisher_->assert_liveliness();
      }
    }

    std::string module_name_;
    bool critical_{false};
    std::string heartbeat_topic_;
    int publish_period_ms_;
    int heartbeat_deadline_ms_;
    int heartbeat_liveliness_ms_;

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
    rclcpp::Service<Trigger>::SharedPtr request_deregister_service_;
    rclcpp::TimerBase::SharedPtr timer_;
  };

  int main(int argc, char ** argv)
  {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<RegistrableTemplateNode>());
    rclcpp::shutdown();
    return 0;
  }

