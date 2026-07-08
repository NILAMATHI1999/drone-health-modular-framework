  #include <chrono>
  #include <cstdint>
  #include <memory>
  #include <stdexcept>
  #include <string>
  #include <vector>
  #include <future>


  #include "rclcpp/rclcpp.hpp"
  #include "sensor_msgs/msg/image.hpp"
  #include "std_msgs/msg/string.hpp"
  #include "drone_health_interfaces/msg/monitor_spec.hpp"
  #include "drone_health_interfaces/srv/deregister_module.hpp"
  #include "drone_health_interfaces/srv/register_module.hpp"
  #include "std_srvs/srv/trigger.hpp"


class SimulatedCameraNode : public rclcpp::Node
{
public:
  SimulatedCameraNode()
  : Node("simulated_camera_node")
  {
    declare_parameters();
    read_parameters();
    setup_qos();
    setup_publishers();
    setup_management_client();
    setup_request_deregister_service();
    setup_mission_phase_subscription();
    setup_timer();

    RCLCPP_INFO(get_logger(), "Simulated camera node started");
  }

private:
  using DeregisterModule = drone_health_interfaces::srv::DeregisterModule;
  using MonitorSpec = drone_health_interfaces::msg::MonitorSpec;
  using RegisterModule = drone_health_interfaces::srv::RegisterModule;
  using Trigger = std_srvs::srv::Trigger;


  void declare_parameters()
  {
    declare_parameter<std::string>("frame_id", "camera_link");
    declare_parameter<int>("publish_period_ms", 100);
    declare_parameter<int>("image_deadline_ms", 250);
    declare_parameter<int>("heartbeat_deadline_ms", 700);
    declare_parameter<int>("heartbeat_liveliness_ms", 1500);
    declare_parameter<int>("image_width", 160);
    declare_parameter<int>("image_height", 120);
  }

  void read_parameters()
  {
    frame_id_ = get_parameter("frame_id").as_string();
    publish_period_ms_ = get_parameter("publish_period_ms").as_int();
    image_deadline_ms_ = get_parameter("image_deadline_ms").as_int();
    heartbeat_deadline_ms_ = get_parameter("heartbeat_deadline_ms").as_int();
    heartbeat_liveliness_ms_ = get_parameter("heartbeat_liveliness_ms").as_int();
    image_width_ = get_parameter("image_width").as_int();
    image_height_ = get_parameter("image_height").as_int();

    if (publish_period_ms_ <= 0) {
      throw std::runtime_error("publish_period_ms must be greater than 0");
    }

    if (image_deadline_ms_ <= publish_period_ms_) {
      throw std::runtime_error("image_deadline_ms must be greater than publish_period_ms");
    }

    if (heartbeat_deadline_ms_ <= 0 ||
      heartbeat_liveliness_ms_ <= heartbeat_deadline_ms_)
    {
      throw std::runtime_error("heartbeat liveliness must be greater than heartbeat deadline");
    }

    if (image_width_ <= 0 || image_height_ <= 0) {
      throw std::runtime_error("image dimensions must be greater than 0");
    }
  }

  void setup_qos()
  {
    image_qos_ = rclcpp::QoS(rclcpp::KeepLast(5))
      .best_effort()
      .deadline(std::chrono::milliseconds(image_deadline_ms_));

    heartbeat_qos_ = rclcpp::QoS(rclcpp::KeepLast(10))
      .reliable()
      .deadline(std::chrono::milliseconds(heartbeat_deadline_ms_))
      .liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC)
      .liveliness_lease_duration(std::chrono::milliseconds(heartbeat_liveliness_ms_));
  }

  void setup_publishers()
  {
    image_publisher_ = create_publisher<sensor_msgs::msg::Image>(
        "/camera/image_raw",
        image_qos_);

    heartbeat_publisher_ = create_publisher<std_msgs::msg::String>(
        "/camera/heartbeat",
        heartbeat_qos_);
  }

  void setup_timer()
  {
    timer_ = create_wall_timer(
        std::chrono::milliseconds(publish_period_ms_),
        std::bind(&SimulatedCameraNode::timer_callback, this));
  }

  void setup_management_client()
  {
    register_client_ = create_client<RegisterModule>(
        "/management/register_module");
    deregister_client_ = create_client<DeregisterModule>(
        "/management/deregister_module");
  }


  void setup_request_deregister_service()
  {
    request_deregister_service_ = create_service<Trigger>(
        "/camera/request_deregister",
        std::bind(
          &SimulatedCameraNode::handle_request_deregister,
          this,
          std::placeholders::_1,
          std::placeholders::_2));
  }

  void handle_request_deregister(
    const std::shared_ptr<Trigger::Request>,
    std::shared_ptr<Trigger::Response> response)
  {
    if (self_deregistered_) {
      response->success = true;
      response->message = "camera is already deregistered";
      return;
    }

    deregistration_requested_ = true;
    shutdown_after_deregister_ = true;

    request_self_deregister("deregistered", true);

    response->success = true;
    response->message = "camera deregistration requested";
  }


  void setup_mission_phase_subscription()
  {
    mission_phase_subscription_ = create_subscription<std_msgs::msg::String>(
        "/mission/phase",
        rclcpp::QoS(rclcpp::KeepLast(10)).reliable(),
        std::bind(
          &SimulatedCameraNode::handle_mission_phase,
          this,
          std::placeholders::_1));
  }


  void handle_mission_phase(const std_msgs::msg::String::SharedPtr msg)
  {
    if (msg->data != "INSPECTION_COMPLETE") {
      return;
    }

    if (self_deregistered_) {
      return;
    }

    deregistration_requested_ = true;
    shutdown_after_deregister_ = true;

    request_self_deregister("deregistered", true);
  }


  void request_self_deregister(const std::string & reason, bool shutdown_after_success)
  {
    if (self_deregistered_ || deregistration_request_pending_) {
      return;
    }

    if (!deregister_client_->service_is_ready()) {
      RCLCPP_WARN_THROTTLE(
          get_logger(),
          *get_clock(),
          2000,
          "Management deregister service not ready; camera will retry deregistration");
      return;
    }

    auto request = std::make_shared<DeregisterModule::Request>();
    request->module_name = "camera";
    request->reason = reason;

    deregistration_request_pending_ = true;
    shutdown_after_deregister_ = shutdown_after_success;

    deregister_client_->async_send_request(
        request,
      [this, reason, shutdown_after_success](
        std::shared_future<DeregisterModule::Response::SharedPtr> future)
      {
        deregistration_request_pending_ = false;

        const auto response = future.get();

        if (!response->success) {
          RCLCPP_WARN(
              get_logger(),
              "Camera self deregistration rejected: %s",
              response->message.c_str());
          return;
        }

        self_deregistered_ = true;
        deregistration_requested_ = false;
        publishing_enabled_ = false;
        shutdown_after_deregister_ = shutdown_after_success;

        RCLCPP_INFO(
            get_logger(),
            "Camera self deregistered: %s",
            reason.c_str());
        });

    RCLCPP_INFO(
        get_logger(),
        "Camera self deregistration requested: %s",
        reason.c_str());
  }


  void timer_callback()
  {
    if (!registered_ && !register_request_pending_) {
      request_register();
    }

    if (deregistration_requested_ && !self_deregistered_) {
      request_self_deregister("deregistered", shutdown_after_deregister_);
    }

    if (!registered_) {
      return;
    }

    if (!publishing_enabled_) {
      if (shutdown_after_deregister_) {
        rclcpp::shutdown();
      }
      return;
    }

    ++frame_count_;
    publish_image();
    publish_heartbeat();
    heartbeat_publisher_->assert_liveliness();
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

  void request_register()
  {
    if (!register_client_->service_is_ready()) {
      RCLCPP_WARN_THROTTLE(
          get_logger(),
          *get_clock(),
          2000,
          "Management register service not ready; camera will retry registration");
      return;
    }

    auto request = std::make_shared<RegisterModule::Request>();
    request->module_name = "camera";
    request->critical = false;

    request->monitors.push_back(
      make_monitor(
        "/camera/heartbeat",
        "heartbeat",
        "std_msgs/msg/String",
        "reliable",
        heartbeat_deadline_ms_,
        heartbeat_liveliness_ms_));

    request->monitors.push_back(
      make_monitor(
        "/camera/image_raw",
        "data",
        "sensor_msgs/msg/Image",
        "best_effort",
        image_deadline_ms_,
        0));

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
              "Camera registration rejected: %s",
              response->message.c_str());
          return;
        }

        registered_ = true;
        self_deregistered_ = false;
        publishing_enabled_ = true;

        RCLCPP_INFO(
            get_logger(),
            "Camera registered dynamically: %s",
            response->message.c_str());
      });
  }


  void publish_image()
  {
    sensor_msgs::msg::Image image;
    image.header.stamp = now();
    image.header.frame_id = frame_id_;
    image.height = static_cast<uint32_t>(image_height_);
    image.width = static_cast<uint32_t>(image_width_);
    image.encoding = "rgb8";
    image.is_bigendian = 0;
    image.step = static_cast<uint32_t>(image_width_ * 3);
    image.data.resize(static_cast<size_t>(image.step * image.height));

    for (int y = 0; y < image_height_; ++y) {
      for (int x = 0; x < image_width_; ++x) {
        const size_t index = static_cast<size_t>((y * image_width_ + x) * 3);
        image.data[index] = static_cast<uint8_t>((x + frame_count_) % 256);
        image.data[index + 1] = static_cast<uint8_t>((y + frame_count_) % 256);
        image.data[index + 2] = static_cast<uint8_t>((x + y + frame_count_) % 256);
      }
    }

    image_publisher_->publish(image);
  }

  void publish_heartbeat()
  {
    std_msgs::msg::String heartbeat;
    heartbeat.data = "simulated_camera_node alive";
    heartbeat_publisher_->publish(heartbeat);
  }

  std::string frame_id_;
  int publish_period_ms_;
  int image_deadline_ms_;
  int heartbeat_deadline_ms_;
  int heartbeat_liveliness_ms_;
  int image_width_;
  int image_height_;
  int frame_count_{0};

  bool publishing_enabled_{true};
  bool registered_{false};
  bool register_request_pending_{false};
  bool self_deregistered_{false};
  bool deregistration_request_pending_{false};
  bool deregistration_requested_{false};
  bool shutdown_after_deregister_{false};


  rclcpp::QoS image_qos_{rclcpp::KeepLast(5)};
  rclcpp::QoS heartbeat_qos_{rclcpp::KeepLast(10)};
  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr image_publisher_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr heartbeat_publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Client<RegisterModule>::SharedPtr register_client_;
  rclcpp::Client<DeregisterModule>::SharedPtr deregister_client_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr mission_phase_subscription_;
  rclcpp::Service<Trigger>::SharedPtr request_deregister_service_;


};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<SimulatedCameraNode>());
  rclcpp::shutdown();
  return 0;
}
