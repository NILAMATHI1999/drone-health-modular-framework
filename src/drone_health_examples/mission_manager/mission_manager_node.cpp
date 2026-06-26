  #include <chrono>
  #include <future>
  #include <memory>
  #include <stdexcept>
  #include <string>

  #include "rclcpp/rclcpp.hpp"
  #include "std_msgs/msg/string.hpp"
  #include "std_srvs/srv/set_bool.hpp"

class MissionManagerNode : public rclcpp::Node
{
public:
  MissionManagerNode()
  : Node("mission_manager_node")
  {
    declare_parameters();
    read_parameters();
    setup_publisher();
    setup_management_client();
    setup_timer();

    node_start_time_ = now();
    RCLCPP_INFO(get_logger(), "Mission manager node started");
  }

private:
  using SetBool = std_srvs::srv::SetBool;

  enum class MissionState
  {
    WAITING_TO_START,
    START_REQUESTED,
    INSPECTION,
    INSPECTION_COMPLETE,
    NAVIGATION_CONTINUE
  };

  void declare_parameters()
  {
    declare_parameter<int>("publish_period_ms", 500);
    declare_parameter<int>("start_delay_s", 5);
    declare_parameter<int>("inspection_duration_s", 50);
  }

  void read_parameters()
  {
    publish_period_ms_ = get_parameter("publish_period_ms").as_int();
    start_delay_s_ = get_parameter("start_delay_s").as_int();
    inspection_duration_s_ = get_parameter("inspection_duration_s").as_int();

    if (publish_period_ms_ <= 0) {
      throw std::runtime_error("publish_period_ms must be greater than 0");
    }

    if (start_delay_s_ < 0) {
      throw std::runtime_error("start_delay_s must be greater than or equal to 0");
    }

    if (inspection_duration_s_ <= 0) {
      throw std::runtime_error("inspection_duration_s must be greater than 0");
    }
  }

  void setup_publisher()
  {
    phase_publisher_ = create_publisher<std_msgs::msg::String>(
        "/mission/phase",
        rclcpp::QoS(rclcpp::KeepLast(10)).reliable());
  }

  void setup_management_client()
  {
    mission_client_ = create_client<SetBool>("/management/set_mission_active");
  }

  void setup_timer()
  {
    timer_ = create_wall_timer(
        std::chrono::milliseconds(publish_period_ms_),
        std::bind(&MissionManagerNode::timer_callback, this));
  }

  void timer_callback()
  {
    publish_phase();

    const double elapsed_since_start_s = (now() - node_start_time_).seconds();

    if (state_ == MissionState::WAITING_TO_START) {
      if (elapsed_since_start_s >= static_cast<double>(start_delay_s_)) {
        request_mission_active(true);
        state_ = MissionState::START_REQUESTED;
      }
      return;
    }

    if (state_ == MissionState::INSPECTION) {
      const double inspection_elapsed_s =
        (now() - inspection_start_time_).seconds();

      if (inspection_elapsed_s >= static_cast<double>(inspection_duration_s_)) {
        state_ = MissionState::INSPECTION_COMPLETE;
        inspection_complete_time_ = now();
        RCLCPP_INFO(get_logger(), "Inspection complete");
      }
      return;
    }

    if (state_ == MissionState::INSPECTION_COMPLETE) {
      const double complete_elapsed_s =
        (now() - inspection_complete_time_).seconds();

      if (complete_elapsed_s >= 2.0) {
        state_ = MissionState::NAVIGATION_CONTINUE;
        RCLCPP_INFO(get_logger(), "Mission continuing after inspection");
      }
      return;
    }
  }

  void request_mission_active(bool active)
  {
    if (request_pending_) {
      return;
    }

    if (!mission_client_->service_is_ready()) {
      RCLCPP_WARN_THROTTLE(
          get_logger(),
          *get_clock(),
          2000,
          "Management mission service not ready; mission manager will retry");
      return;
    }

    auto request = std::make_shared<SetBool::Request>();
    request->data = active;
    request_pending_ = true;

    mission_client_->async_send_request(
        request,
      [this, active](std::shared_future<SetBool::Response::SharedPtr> future)
      {
        request_pending_ = false;
        const auto response = future.get();

        if (!response->success) {
          RCLCPP_WARN(
              get_logger(),
              "Mission active request rejected: %s",
              response->message.c_str());

          if (active) {
            state_ = MissionState::WAITING_TO_START;
          }
          return;
        }

        RCLCPP_INFO(
            get_logger(),
            "Mission active request accepted: %s",
            response->message.c_str());

        if (active) {
          state_ = MissionState::INSPECTION;
          inspection_start_time_ = now();
        }
        });
  }

  void publish_phase()
  {
    std_msgs::msg::String phase;

    switch (state_) {
      case MissionState::WAITING_TO_START:
      case MissionState::START_REQUESTED:
        phase.data = "IDLE";
        break;
      case MissionState::INSPECTION:
        phase.data = "INSPECTION";
        break;
      case MissionState::INSPECTION_COMPLETE:
        phase.data = "INSPECTION_COMPLETE";
        break;
      case MissionState::NAVIGATION_CONTINUE:
        phase.data = "NAVIGATION_CONTINUE";
        break;
    }

    phase_publisher_->publish(phase);
  }

  int publish_period_ms_;
  int start_delay_s_;
  int inspection_duration_s_;

  bool request_pending_{false};
  MissionState state_{MissionState::WAITING_TO_START};

  rclcpp::Time node_start_time_{0, 0, RCL_ROS_TIME};
  rclcpp::Time inspection_start_time_{0, 0, RCL_ROS_TIME};
  rclcpp::Time inspection_complete_time_{0, 0, RCL_ROS_TIME};

  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr phase_publisher_;
  rclcpp::Client<SetBool>::SharedPtr mission_client_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MissionManagerNode>());
  rclcpp::shutdown();
  return 0;
}
