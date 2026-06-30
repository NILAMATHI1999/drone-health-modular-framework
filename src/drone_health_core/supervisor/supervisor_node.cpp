#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>


#include "drone_health_interfaces/msg/health_status.hpp"
#include "drone_health_interfaces/msg/safety_status.hpp"
#include "drone_health_interfaces/msg/supervisor_status.hpp"
#include "drone_health_interfaces/msg/management_state.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "std_srvs/srv/trigger.hpp"


class SupervisorNode : public rclcpp::Node
{
public:
  SupervisorNode()
  : Node("supervisor_node")
  {
    declare_parameters();
    read_parameters();
    validate_parameters();
    setup_heartbeat_qos();
    setup_communication();

    RCLCPP_INFO(get_logger(), "Supervisor node started");
  }

private:
  using HealthStatus = drone_health_interfaces::msg::HealthStatus;
  using SafetyStatus = drone_health_interfaces::msg::SafetyStatus;
  using SupervisorStatus = drone_health_interfaces::msg::SupervisorStatus;
  using ManagementState = drone_health_interfaces::msg::ManagementState;
  using Trigger = std_srvs::srv::Trigger;


  void declare_parameters()
  {
    declare_parameter<std::string>("safety_status_topic", "/safety/status");
    declare_parameter<std::string>("health_status_topic", "/health/status");
    declare_parameter<std::string>("supervisor_status_topic", "/supervisor/status");
    declare_parameter<std::string>("management_state_topic", "/management/state");
    declare_parameter<std::string>("network_status_topic", "/network_status");


    declare_parameter<std::string>("heartbeat_topic", "/supervisor/heartbeat");

    declare_parameter<int>("evaluation_period_ms", 100);
    declare_parameter<int>("safety_status_timeout_ms", 500);
    declare_parameter<int>("health_status_timeout_ms", 1500);
    declare_parameter<int>("management_state_timeout_ms", 1500);
    declare_parameter<int>("network_status_timeout_ms", 3000);
    declare_parameter<int>("network_failsafe_delay_ms", 10000);

    declare_parameter<std::vector<std::string>>(
      "required_health_topics",
      std::vector<std::string>{});

    declare_parameter<int>("heartbeat_period_ms", 500);
    declare_parameter<int>("heartbeat_deadline_ms", 700);
    declare_parameter<int>("heartbeat_liveliness_ms", 1500);
  }

  void read_parameters()
  {
    safety_status_topic_ = get_parameter("safety_status_topic").as_string();
    health_status_topic_ = get_parameter("health_status_topic").as_string();
    supervisor_status_topic_ = get_parameter("supervisor_status_topic").as_string();
    management_state_topic_ = get_parameter("management_state_topic").as_string();
    network_status_topic_ = get_parameter("network_status_topic").as_string();
    management_state_timeout_ms_ = get_parameter("management_state_timeout_ms").as_int();
    network_status_timeout_ms_ = get_parameter("network_status_timeout_ms").as_int();
    network_failsafe_delay_ms_ = get_parameter("network_failsafe_delay_ms").as_int();


    heartbeat_topic_ = get_parameter("heartbeat_topic").as_string();

    evaluation_period_ms_ = get_parameter("evaluation_period_ms").as_int();
    safety_status_timeout_ms_ = get_parameter("safety_status_timeout_ms").as_int();
    health_status_timeout_ms_ = get_parameter("health_status_timeout_ms").as_int();
    required_health_topics_ = get_parameter("required_health_topics").as_string_array();

    heartbeat_period_ms_ = get_parameter("heartbeat_period_ms").as_int();
    heartbeat_deadline_ms_ = get_parameter("heartbeat_deadline_ms").as_int();
    heartbeat_liveliness_ms_ = get_parameter("heartbeat_liveliness_ms").as_int();
  }

  void validate_parameters() const
  {
    if (safety_status_topic_.empty() ||
      health_status_topic_.empty() ||
      supervisor_status_topic_.empty() ||
      management_state_topic_.empty() ||
      network_status_topic_.empty() ||
      heartbeat_topic_.empty())
    {
      throw std::runtime_error("supervisor topic parameters must not be empty");
    }

    if (evaluation_period_ms_ <= 0 ||
      safety_status_timeout_ms_ <= 0 ||
      health_status_timeout_ms_ <= 0 ||
      management_state_timeout_ms_ <= 0 ||
      network_status_timeout_ms_ <= 0 ||
      network_failsafe_delay_ms_ <= 0)
    {
      throw std::runtime_error("supervisor timing parameters must be greater than 0");
    }

    if (required_health_topics_.empty()) {
      throw std::runtime_error("required_health_topics must not be empty");
    }

    if (heartbeat_period_ms_ <= 0 ||
      heartbeat_deadline_ms_ <= heartbeat_period_ms_ ||
      heartbeat_liveliness_ms_ <= heartbeat_deadline_ms_)
    {
      throw std::runtime_error(
        "heartbeat timing must satisfy period < deadline < liveliness");
    }
  }

  void setup_heartbeat_qos()
  {
    heartbeat_qos_ = rclcpp::QoS(rclcpp::KeepLast(10))
      .reliable()
      .deadline(std::chrono::milliseconds(heartbeat_deadline_ms_))
      .liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC)
      .liveliness_lease_duration(
        std::chrono::milliseconds(heartbeat_liveliness_ms_));
  }

  void setup_communication()
  {
    safety_subscription_ = create_subscription<SafetyStatus>(
      safety_status_topic_,
      rclcpp::QoS(rclcpp::KeepLast(10)).reliable(),
      std::bind(&SupervisorNode::handle_safety_status, this, std::placeholders::_1));

    health_subscription_ = create_subscription<HealthStatus>(
      health_status_topic_,
      rclcpp::QoS(rclcpp::KeepLast(10)).reliable(),
      std::bind(&SupervisorNode::handle_health_status, this, std::placeholders::_1));

    management_subscription_ = create_subscription<ManagementState>(
      management_state_topic_,
      rclcpp::QoS(rclcpp::KeepLast(10)).reliable(),
      std::bind(&SupervisorNode::handle_management_state, this,
      std::placeholders::_1));

    network_subscription_ = create_subscription<std_msgs::msg::String>(
      network_status_topic_,
      rclcpp::QoS(rclcpp::KeepLast(10)).reliable(),
      std::bind(&SupervisorNode::handle_network_status, this, std::placeholders::_1));


    supervisor_publisher_ = create_publisher<SupervisorStatus>(
      supervisor_status_topic_,
      rclcpp::QoS(rclcpp::KeepLast(10)).reliable());

    heartbeat_publisher_ = create_publisher<std_msgs::msg::String>(
      heartbeat_topic_,
      heartbeat_qos_);

    evaluation_timer_ = create_wall_timer(
      std::chrono::milliseconds(evaluation_period_ms_),
      std::bind(&SupervisorNode::evaluate_supervisor_state, this));

    heartbeat_timer_ = create_wall_timer(
      std::chrono::milliseconds(heartbeat_period_ms_),
      std::bind(&SupervisorNode::publish_heartbeat, this));

    reset_service_ = create_service<std_srvs::srv::Trigger>(
    "/supervisor/reset_emergency_stop",
    std::bind(
      &SupervisorNode::handle_reset_emergency_stop,
      this,
      std::placeholders::_1,
      std::placeholders::_2));
  }

  void handle_safety_status(const SafetyStatus::SharedPtr msg)
  {
    latest_safety_status_ = *msg;
    has_safety_status_ = true;
    last_safety_status_time_ = now();
  }

  void handle_health_status(const HealthStatus::SharedPtr msg)
  {
    has_health_status_ = true;
    last_health_status_time_ = now();
    latest_health_by_topic_[msg->topic_name] = *msg;
  }

  void handle_management_state(const ManagementState::SharedPtr msg)
  {
    last_management_state_time_ = now();
    has_management_state_ = true;
    maintenance_mode_ = msg->maintenance_mode;
    mission_active_ = msg->mission_active;
    planned_inactive_topics_.clear();

    const auto count = std::min(
      msg->planned_inactive_topics.size(),
      msg->planned_inactive_topic_reasons.size());

    for (size_t i = 0; i < count; ++i) {
      planned_inactive_topics_[msg->planned_inactive_topics[i]] =
        msg->planned_inactive_topic_reasons[i];
    }
  }

  void handle_network_status(const std_msgs::msg::String::SharedPtr msg)
  {
    latest_network_status_ = msg->data;
    has_network_status_ = true;
    last_network_status_time_ = now();
  }


  void evaluate_supervisor_state()
  {
    SupervisorStatus status;
    status.header.stamp = now();
    status.header.frame_id = "base_link";

    if (!has_safety_status_) {
      status.mode = SupervisorStatus::UNKNOWN;
      status.reason = SupervisorStatus::REASON_WAITING_FOR_SAFETY;
      status.command_allowed = false;
      status.message = "waiting for safety status";
      supervisor_publisher_->publish(status);
      return;
    }

    if (!safety_status_fresh()) {
      status.mode = SupervisorStatus::FAILSAFE;
      status.reason = SupervisorStatus::REASON_SAFETY_STATUS_STALE;
      status.command_allowed = false;
      status.message = "safety status stale";
      supervisor_publisher_->publish(status);
      return;
    }

    if (!management_state_fresh()) {
      status.mode = mission_active_ ?
        SupervisorStatus::FAILSAFE :
        SupervisorStatus::HOLD;
      status.reason = SupervisorStatus::REASON_MANAGEMENT_STATUS_STALE;
      status.command_allowed = false;
      status.message = "management state is stale";
      supervisor_publisher_->publish(status);
      return;
    }

    const bool network_available = network_status_fresh() && network_status_ok();
    if (!network_available) {
      if (network_unavailable_since_.nanoseconds() == 0) {
        network_unavailable_since_ = now();
      }

      const double unavailable_s = (now() - network_unavailable_since_).seconds();
      const double failsafe_delay_s =
        static_cast<double>(network_failsafe_delay_ms_) / 1000.0;

      status.mode = unavailable_s >= failsafe_delay_s ?
        SupervisorStatus::FAILSAFE :
        SupervisorStatus::HOLD;
      status.reason = network_status_fresh() ?
        SupervisorStatus::REASON_NETWORK_UNHEALTHY :
        SupervisorStatus::REASON_NETWORK_STATUS_STALE;
      status.command_allowed = false;
      status.message = unavailable_s >= failsafe_delay_s ?
        "network lost for more than failsafe delay" :
        "waiting for network recovery";
      supervisor_publisher_->publish(status);
      return;
    }

    network_unavailable_since_ = rclcpp::Time(0, 0, RCL_ROS_TIME);

    if (!health_status_fresh()) {
      status.mode = mission_active_ ?
        SupervisorStatus::FAILSAFE :
        SupervisorStatus::HOLD;
      status.reason = SupervisorStatus::REASON_HEALTH_STATUS_STALE;
      status.command_allowed = false;
      status.message = "health status is stale";
      supervisor_publisher_->publish(status);
      return;
    }

    if (latest_safety_status_.state == SafetyStatus::UNSAFE &&
      latest_safety_status_.reason == SafetyStatus::REASON_INSUFFICIENT_BRAKING_DISTANCE)
    {
      emergency_stop_latched_ = true;
      status.mode = SupervisorStatus::EMERGENCY_STOP;
      status.reason = SupervisorStatus::REASON_OBSTACLE_TOO_CLOSE;
      status.command_allowed = false;
      status.message = "obstacle too close for braking distance";
      supervisor_publisher_->publish(status);
      return;
    }

    if (emergency_stop_latched_) {
      status.mode = SupervisorStatus::EMERGENCY_STOP;
      status.reason = SupervisorStatus::REASON_OBSTACLE_TOO_CLOSE;
      status.command_allowed = false;
      status.message = "emergency stop latched";
      supervisor_publisher_->publish(status);
      return;
    }

    std::string health_message;
    if (required_health_topics_waiting(health_message)) {
      status.mode = mission_active_ ?
        SupervisorStatus::FAILSAFE :
        SupervisorStatus::HOLD;
      status.reason = SupervisorStatus::REASON_HEALTH_STATUS_STALE;
      status.command_allowed = false;
      status.message = health_message;
      supervisor_publisher_->publish(status);
      return;
    }

    if (!required_health_topics_ok(health_message)) {
      status.mode = mission_active_ ?
        SupervisorStatus::FAILSAFE :
        SupervisorStatus::HOLD;
      status.reason = SupervisorStatus::REASON_REQUIRED_HEALTH_FAILED;
      status.command_allowed = false;
      status.message = health_message;
      supervisor_publisher_->publish(status);
      return;
    }

    if (maintenance_mode_) {
      status.mode = SupervisorStatus::HOLD;
      status.reason = SupervisorStatus::REASON_MAINTENANCE_MODE;
      status.command_allowed = false;
      status.message = "maintenance mode active";
      supervisor_publisher_->publish(status);
      return;
    }

    std::string planned_inactive_message;
    if (planned_inactive_required_topics(planned_inactive_message)) {
      status.mode = mission_active_ ?
        SupervisorStatus::FAILSAFE :
        SupervisorStatus::HOLD;
      status.reason = SupervisorStatus::REASON_PLANNED_INACTIVE;
      status.command_allowed = false;
      status.message = planned_inactive_message;
      supervisor_publisher_->publish(status);
      return;
    }

    if (latest_safety_status_.state == SafetyStatus::UNSAFE &&
      latest_safety_status_.reason == SafetyStatus::REASON_HEALTH_UNSAFE)
    {
      status.mode = mission_active_ ?
        SupervisorStatus::FAILSAFE :
        SupervisorStatus::HOLD;
      status.reason = SupervisorStatus::REASON_HEALTH_UNSAFE;
      status.command_allowed = false;
      status.message = "required system health is unsafe";
    } else if (latest_safety_status_.state == SafetyStatus::UNKNOWN &&
      latest_safety_status_.reason == SafetyStatus::REASON_INVALID_INPUT)
    {
      status.mode = SupervisorStatus::HOLD;
      status.reason = SupervisorStatus::REASON_INVALID_SAFETY_INPUT;
      status.command_allowed = false;
      status.message = "invalid safety input";
    } else if (latest_safety_status_.state == SafetyStatus::UNKNOWN) {
      status.mode = SupervisorStatus::HOLD;
      status.reason = SupervisorStatus::REASON_SAFETY_UNKNOWN;
      status.command_allowed = false;
      status.message = "safety status unknown";
    } else if (latest_safety_status_.state == SafetyStatus::SAFE) {
      status.mode = SupervisorStatus::NORMAL;
      status.reason = SupervisorStatus::REASON_NONE;
      status.command_allowed = true;
      status.message = "system normal";
    } else {
      status.mode = SupervisorStatus::HOLD;
      status.reason = SupervisorStatus::REASON_INVALID_SAFETY_INPUT;
      status.command_allowed = false;
      status.message = "unhandled safety status";
    }

    supervisor_publisher_->publish(status);
  }


  bool safety_status_fresh() const
  {
    if (!has_safety_status_) {
      return false;
    }

    const double age_s = (now() - last_safety_status_time_).seconds();
    const double timeout_s = static_cast<double>(safety_status_timeout_ms_) / 1000.0;

    return age_s <= timeout_s;
  }

  bool health_status_fresh() const
  {
    if (!has_health_status_) {
      return false;
    }

    const double age_s = (now() - last_health_status_time_).seconds();
    const double timeout_s = static_cast<double>(health_status_timeout_ms_) / 1000.0;

    return age_s <= timeout_s;
  }

  bool management_state_fresh() const
  {
    if (!has_management_state_) {
      return false;
    }

    const double age_s = (now() - last_management_state_time_).seconds();
    const double timeout_s =
      static_cast<double>(management_state_timeout_ms_) / 1000.0;

    return age_s <= timeout_s;
  }

  bool network_status_fresh() const
  {
    if (!has_network_status_) {
      return false;
    }

    const double age_s = (now() - last_network_status_time_).seconds();
    const double timeout_s =
      static_cast<double>(network_status_timeout_ms_) / 1000.0;

    return age_s <= timeout_s;
  }

  bool network_status_ok() const
  {
    return latest_network_status_ == "NETWORK_HEALTHY" ||
           latest_network_status_ == "NETWORK_BACKUP";
  }

  bool required_health_topics_waiting(std::string & message) const
  {
    int waiting_count = 0;
    std::string first_waiting;

    for (const auto & topic : required_health_topics_) {
      if (planned_inactive_topics_.find(topic) !=
        planned_inactive_topics_.end())
      {
        continue;
      }

      const auto item = latest_health_by_topic_.find(topic);

      if (item == latest_health_by_topic_.end() ||
        item->second.status == HealthStatus::UNKNOWN)
      {
        ++waiting_count;

        if (first_waiting.empty()) {
          first_waiting = "waiting for required health topic " + topic;
        }

        continue;
      }

      if (item->second.status != HealthStatus::OK) {
        return false;
      }
    }

    if (waiting_count == 0) {
      return false;
    }

    if (waiting_count == 1) {
      message = first_waiting;
    } else {
      message =
        "waiting for " + std::to_string(waiting_count) + " required health topics";
    }

    return true;
  }

  bool required_health_topics_ok(std::string & failure_message) const
  {
    int failed_count = 0;
    std::string first_failure;

    for (const auto & topic : required_health_topics_) {
      if (planned_inactive_topics_.find(topic) !=
        planned_inactive_topics_.end())
      {
        continue;
      }

      const auto item = latest_health_by_topic_.find(topic);

      if (item == latest_health_by_topic_.end()) {
        continue;
      }

      const auto & health = item->second;

      if (health.status == HealthStatus::UNKNOWN) {
        continue;
      }

      if (health.status != HealthStatus::OK) {
        ++failed_count;

        if (first_failure.empty()) {
          first_failure =
            "required health failed: " + topic + " - " + health.message;
        }
      }
    }

    if (failed_count == 0) {
      return true;
    }

    if (failed_count == 1) {
      failure_message = first_failure;
    } else {
      failure_message =
        std::to_string(failed_count) + " required health topics failed";
    }

    return false;
  }

  bool planned_inactive_required_topics(std::string & message) const
  {
    int inactive_count = 0;
    std::string first_inactive;

    for (const auto & topic : required_health_topics_) {
      const auto item = planned_inactive_topics_.find(topic);

      if (item == planned_inactive_topics_.end()) {
        continue;
      }

      ++inactive_count;

      if (first_inactive.empty()) {
        first_inactive = "planned inactive: " + topic + " - " +
          item->second;
      }
    }

    if (inactive_count == 0) {
      return false;
    }

    if (inactive_count == 1) {
      message = first_inactive;
    } else {
      message = std::to_string(inactive_count) + " planned inactive required topics";
    }

    return true;
  }


  void handle_reset_emergency_stop(
    const std::shared_ptr<std_srvs::srv::Trigger::Request>,
    std::shared_ptr<std_srvs::srv::Trigger::Response> response)
  {
    if (!emergency_stop_latched_) {
      response->success = true;
      response->message = "emergency stop is not latched";
      return;
    }

    std::string failed_health_message;
    if (!safety_status_fresh() ||
      !health_status_fresh() ||
      !network_status_fresh() ||
      !network_status_ok() ||
      latest_safety_status_.state != SafetyStatus::SAFE ||
      maintenance_mode_ ||
      planned_inactive_required_topics(failed_health_message) ||
      !required_health_topics_ok(failed_health_message))
    {
      response->success = false;
      response->message =
        "reset rejected: safety must be fresh, safe, and healthy";
      return;
    }

    emergency_stop_latched_ = false;
    response->success = true;
    response->message = "emergency stop latch reset";
  }

  void publish_heartbeat()
  {
    std_msgs::msg::String heartbeat;
    heartbeat.data = "supervisor_node alive";
    heartbeat_publisher_->publish(heartbeat);
    if (!heartbeat_publisher_->assert_liveliness()) {
      RCLCPP_WARN(get_logger(), "Failed to assert supervisor liveliness");
    }
  }

  std::string safety_status_topic_;
  std::string health_status_topic_;
  std::string supervisor_status_topic_;
  std::string heartbeat_topic_;
  std::string management_state_topic_;
  std::string network_status_topic_;

  int evaluation_period_ms_;
  int safety_status_timeout_ms_;
  int health_status_timeout_ms_;
  int management_state_timeout_ms_{1500};
  int network_status_timeout_ms_{3000};
  int network_failsafe_delay_ms_{10000};

  std::vector<std::string> required_health_topics_;

  int heartbeat_period_ms_;
  int heartbeat_deadline_ms_;
  int heartbeat_liveliness_ms_;

  bool has_safety_status_{false};
  bool has_health_status_{false};
  bool emergency_stop_latched_{false};
  bool maintenance_mode_{false};
  bool mission_active_{false};
  bool has_management_state_{false};
  bool has_network_status_{false};


  SafetyStatus latest_safety_status_;
  std::unordered_map<std::string, HealthStatus> latest_health_by_topic_;
  std::unordered_map<std::string, std::string> planned_inactive_topics_;
  std::string latest_network_status_{"UNKNOWN"};


  rclcpp::Time last_safety_status_time_{0, 0, RCL_ROS_TIME};
  rclcpp::Time last_health_status_time_{0, 0, RCL_ROS_TIME};
  rclcpp::Time last_management_state_time_{0, 0, RCL_ROS_TIME};
  rclcpp::Time last_network_status_time_{0, 0, RCL_ROS_TIME};
  rclcpp::Time network_unavailable_since_{0, 0, RCL_ROS_TIME};


  rclcpp::QoS heartbeat_qos_{rclcpp::KeepLast(10)};

  rclcpp::Subscription<SafetyStatus>::SharedPtr safety_subscription_;
  rclcpp::Subscription<HealthStatus>::SharedPtr health_subscription_;

  rclcpp::Publisher<SupervisorStatus>::SharedPtr supervisor_publisher_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr heartbeat_publisher_;

  rclcpp::TimerBase::SharedPtr evaluation_timer_;
  rclcpp::TimerBase::SharedPtr heartbeat_timer_;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr reset_service_;
  rclcpp::Subscription<ManagementState>::SharedPtr management_subscription_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr network_subscription_;


};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<SupervisorNode>());
  rclcpp::shutdown();
  return 0;
}
