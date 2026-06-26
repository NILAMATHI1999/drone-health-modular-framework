#include <algorithm>
  #include <chrono>
  #include <cmath>
  #include <memory>
  #include <stdexcept>
  #include <string>
  #include <unordered_map>
  #include <vector>
  #include <limits>


  #include "drone_health_interfaces/msg/health_status.hpp"
  #include "drone_health_interfaces/msg/safety_status.hpp"
  #include "geometry_msgs/msg/twist_stamped.hpp"
  #include "rclcpp/rclcpp.hpp"
  #include "std_msgs/msg/float32.hpp"
  #include "std_msgs/msg/string.hpp"


  class SafetyFusionNode : public rclcpp::Node
  {
  public:
    SafetyFusionNode()
    : Node("safety_fusion_node")
    {
      declare_parameters();
      read_parameters();
      validate_parameters();
      setup_heartbeat_qos();
      setup_communication();

      RCLCPP_INFO(get_logger(), "Safety fusion node started");
    }

  private:
    using HealthStatus = drone_health_interfaces::msg::HealthStatus;
    using SafetyStatus = drone_health_interfaces::msg::SafetyStatus;

    struct TopicHealth
    {
      uint8_t status{HealthStatus::UNKNOWN};
      uint8_t reason{HealthStatus::REASON_NONE};
      bool seen{false};
    };

    void declare_parameters()
    {
      declare_parameter<std::string>("nearest_obstacle_topic", "/lidar/nearest_obstacle");
      declare_parameter<std::string>("velocity_topic", "/vehicle/velocity");
      declare_parameter<std::string>("health_status_topic", "/health/status");
      declare_parameter<int>("health_status_timeout_ms", 1500);
      declare_parameter<int>("obstacle_timeout_ms", 700);
      declare_parameter<int>("velocity_timeout_ms", 700);




      declare_parameter<std::string>("safety_status_topic", "/safety/status");

      declare_parameter<std::vector<std::string>>(
        "required_health_topics",
        std::vector<std::string>{
          "/lidar/nearest_obstacle",
          "/vehicle/velocity"
        });

      declare_parameter<int>("evaluation_period_ms", 100);
      declare_parameter<double>("max_deceleration_mps2", 1.5);
      declare_parameter<double>("reaction_time_s", 0.2);
      declare_parameter<double>("safety_margin_m", 0.5);
      declare_parameter<std::string>("heartbeat_topic", "/safety_fusion/heartbeat");
      declare_parameter<int>("heartbeat_period_ms", 500);
      declare_parameter<int>("heartbeat_deadline_ms", 700);
      declare_parameter<int>("heartbeat_liveliness_ms", 1500);


    }

    void read_parameters()
    {
      nearest_obstacle_topic_ = get_parameter("nearest_obstacle_topic").as_string();
      velocity_topic_ = get_parameter("velocity_topic").as_string();
      health_status_topic_ = get_parameter("health_status_topic").as_string();
      health_status_timeout_ms_ = get_parameter("health_status_timeout_ms").as_int();
      obstacle_timeout_ms_ = get_parameter("obstacle_timeout_ms").as_int();
      velocity_timeout_ms_ = get_parameter("velocity_timeout_ms").as_int();




      safety_status_topic_ = get_parameter("safety_status_topic").as_string();

      required_health_topics_ = get_parameter("required_health_topics").as_string_array();

      evaluation_period_ms_ = get_parameter("evaluation_period_ms").as_int();
      max_deceleration_mps2_ = get_parameter("max_deceleration_mps2").as_double();
      reaction_time_s_ = get_parameter("reaction_time_s").as_double();
      safety_margin_m_ = get_parameter("safety_margin_m").as_double();
      heartbeat_topic_ = get_parameter("heartbeat_topic").as_string();

      heartbeat_period_ms_ = get_parameter("heartbeat_period_ms").as_int();
      heartbeat_deadline_ms_ = get_parameter("heartbeat_deadline_ms").as_int();
      heartbeat_liveliness_ms_ = get_parameter("heartbeat_liveliness_ms").as_int();


    }

    void validate_parameters() const
    {
      if (nearest_obstacle_topic_.empty() || velocity_topic_.empty() ||health_status_topic_.empty() || safety_status_topic_.empty())
      {
        throw std::runtime_error("topic parameters must not be empty");
      }

      if (required_health_topics_.empty()) {
        throw std::runtime_error("required_health_topics must not be empty");
      }

      if (heartbeat_topic_.empty()) {
    throw std::runtime_error("heartbeat_topic must not be empty");
     }

     if (heartbeat_period_ms_ <= 0 ||
      heartbeat_deadline_ms_ <= heartbeat_period_ms_ ||
      heartbeat_liveliness_ms_ <= heartbeat_deadline_ms_)
     {
    throw std::runtime_error(
      "heartbeat timing must satisfy period < deadline < liveliness");
    }
    if (health_status_timeout_ms_ <= 0 ||
    obstacle_timeout_ms_ <= 0 ||
    velocity_timeout_ms_ <= 0)
  {
    throw std::runtime_error("input timeout parameters must be greater than 0");
  }





      if (evaluation_period_ms_ <= 0 || max_deceleration_mps2_ <= 0.0 || reaction_time_s_ < 0.0 || safety_margin_m_ < 0.0)
      {
        throw std::runtime_error("invalid safety fusion parameter values");
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
      nearest_obstacle_subscription_ =
      create_subscription<std_msgs::msg::Float32>(
        nearest_obstacle_topic_,
        rclcpp::QoS(rclcpp::KeepLast(10)).reliable(),
        std::bind(
          &SafetyFusionNode::handle_nearest_obstacle,
          this,
          std::placeholders::_1));

      velocity_subscription_ =
      create_subscription<geometry_msgs::msg::TwistStamped>(
        velocity_topic_,
        rclcpp::QoS(rclcpp::KeepLast(10)).best_effort(),
        std::bind(
          &SafetyFusionNode::handle_velocity,
          this,
          std::placeholders::_1));

      health_subscription_ = create_subscription<HealthStatus>(
        health_status_topic_,
        rclcpp::QoS(rclcpp::KeepLast(10)).reliable(),
        std::bind(
          &SafetyFusionNode::handle_health_status,
          this,
          std::placeholders::_1));

      safety_publisher_ = create_publisher<SafetyStatus>(
        safety_status_topic_,
        rclcpp::QoS(rclcpp::KeepLast(10)).reliable());

        heartbeat_publisher_ = create_publisher<std_msgs::msg::String>(
    heartbeat_topic_,
    heartbeat_qos_);

  heartbeat_timer_ = create_wall_timer(
    std::chrono::milliseconds(heartbeat_period_ms_),
    std::bind(&SafetyFusionNode::publish_heartbeat, this));



      evaluation_timer_ = create_wall_timer(
        std::chrono::milliseconds(evaluation_period_ms_),
        std::bind(&SafetyFusionNode::evaluate_safety, this));
    }
   void publish_heartbeat()
  {
    std_msgs::msg::String heartbeat;
    heartbeat.data = "safety_fusion_node alive";
    heartbeat_publisher_->publish(heartbeat);
    if (!heartbeat_publisher_->assert_liveliness()) {
      RCLCPP_WARN(get_logger(), "Failed to assert safety fusion liveliness");
    }
  }

    void handle_nearest_obstacle(const std_msgs::msg::Float32::SharedPtr msg)
    {
      nearest_obstacle_m_ = msg->data;
      has_nearest_obstacle_ = true;
      last_obstacle_time_ = now();

    }

    void handle_velocity(const geometry_msgs::msg::TwistStamped::SharedPtr msg)
    {
      const auto & linear = msg->twist.linear;

      speed_mps_ = std::sqrt(
        linear.x * linear.x +
        linear.y * linear.y +
        linear.z * linear.z);

      has_velocity_ = true;
      last_velocity_time_ = now();

    }

    void handle_health_status(const HealthStatus::SharedPtr msg)
    {
      auto & health = topic_health_[msg->topic_name];
      health.status = msg->status;
      health.reason = msg->reason;
      health.seen = true;

      last_health_status_time_ = now();
      has_health_status_ = true;


    }
  // Check whether HealthMonitor is still publishing fresh /health/status messages:Is the latest health status still fresh and trustworthy?

  bool health_status_fresh() const
  {
    if (!has_health_status_) {
      return false;
    }

    const double age_s = (now() - last_health_status_time_).seconds();
    const double timeout_s =
      static_cast<double>(health_status_timeout_ms_) / 1000.0;

    return age_s <= timeout_s;
  }

   bool obstacle_fresh() const
    {
      if (!has_nearest_obstacle_) {
        return false;
      }

      const double age_s = (now() - last_obstacle_time_).seconds();
      const double timeout_s = static_cast<double>(obstacle_timeout_ms_) / 1000.0;

      return age_s <= timeout_s;
    }

    bool velocity_fresh() const
    {
      if (!has_velocity_) {
        return false;
      }

      const double age_s = (now() - last_velocity_time_).seconds();
      const double timeout_s = static_cast<double>(velocity_timeout_ms_) / 1000.0;

      return age_s <= timeout_s;
    }

 void evaluate_safety()
  {
    SafetyStatus status;
    status.header.stamp = now();
    status.header.frame_id = "base_link";

    status.nearest_obstacle_m =
      has_nearest_obstacle_ ? nearest_obstacle_m_ :
      std::numeric_limits<float>::quiet_NaN();
    status.speed_mps =
      has_velocity_ ? speed_mps_ : std::numeric_limits<float>::quiet_NaN();
    status.effective_deceleration_mps2 = static_cast<float>(max_deceleration_mps2_);
    status.safety_margin_m = static_cast<float>(safety_margin_m_);
    status.braking_distance_m = std::numeric_limits<float>::quiet_NaN();
    status.reaction_distance_m = std::numeric_limits<float>::quiet_NaN();
    status.required_clearance_m = std::numeric_limits<float>::quiet_NaN();

    if (!has_nearest_obstacle_ || !has_velocity_ || !
    required_health_seen()) {
      status.state = SafetyStatus::UNKNOWN;
      status.reason = SafetyStatus::REASON_WAITING_FOR_INPUTS;
      safety_publisher_->publish(status);
      return;
    }

    if (!obstacle_fresh() || !velocity_fresh()) {
      status.state = SafetyStatus::UNSAFE;
      status.reason = SafetyStatus::REASON_HEALTH_UNSAFE;
      safety_publisher_->publish(status);
      return;
    }

    if (!input_values_valid()) {
      status.state = SafetyStatus::UNKNOWN;
      status.reason = SafetyStatus::REASON_INVALID_INPUT;
      safety_publisher_->publish(status);
      return;
    }

    if (!health_status_fresh()) {
      status.state = SafetyStatus::UNSAFE;
      status.reason = SafetyStatus::REASON_HEALTH_UNSAFE;
      safety_publisher_->publish(status);
      return;
    }

    if (!required_health_ok()) {
      status.state = SafetyStatus::UNSAFE;
      status.reason = SafetyStatus::REASON_HEALTH_UNSAFE;
      safety_publisher_->publish(status);
      return;
    }

    const double braking_distance = (speed_mps_ * speed_mps_) / (2.0 * max_deceleration_mps2_);
    const double reaction_distance = speed_mps_ * reaction_time_s_;
    const double required_clearance = braking_distance + reaction_distance + safety_margin_m_;

    status.braking_distance_m = static_cast<float>(braking_distance);
    status.reaction_distance_m = static_cast<float>(reaction_distance);
    status.required_clearance_m = static_cast<float>(required_clearance);

    if (nearest_obstacle_m_ <= required_clearance) {
      status.state = SafetyStatus::UNSAFE;
      status.reason = SafetyStatus::REASON_INSUFFICIENT_BRAKING_DISTANCE;
    } else {
      status.state = SafetyStatus::SAFE;
      status.reason = SafetyStatus::REASON_NONE;
    }

    safety_publisher_->publish(status);
  }


    bool input_values_valid() const
    {
      return std::isfinite(nearest_obstacle_m_) &&
        std::isfinite(speed_mps_) &&
        nearest_obstacle_m_ >= 0.0F &&
        speed_mps_ >= 0.0F;
    }
// Check whether health information has been received for all required topics(velocity,nearest obstacle).:Have all required topics reported health at least once?
bool required_health_seen() const
    {
      return std::all_of(
        required_health_topics_.begin(),
        required_health_topics_.end(),
        [this](const std::string & topic) {
          const auto item = topic_health_.find(topic);
          return item != topic_health_.end() && item->second.seen;
        });
    }

// Check whether all required topics(velocity,nearest obstacle) are currently healthy.:Are all required topics currently in OK health state?
    bool required_health_ok() const
    {
      return std::all_of(
        required_health_topics_.begin(),
        required_health_topics_.end(),
        [this](const std::string & topic) {
          const auto item = topic_health_.find(topic);
          return item != topic_health_.end() &&
            item->second.status == HealthStatus::OK;
        });
    }

    std::string nearest_obstacle_topic_;
    std::string velocity_topic_;
    std::string health_status_topic_;
    std::string safety_status_topic_;
    std::vector<std::string> required_health_topics_;
    std::string heartbeat_topic_;


    int evaluation_period_ms_;
    double max_deceleration_mps2_;
    double reaction_time_s_;
    double safety_margin_m_;
    int heartbeat_period_ms_;
    int heartbeat_deadline_ms_;
    int heartbeat_liveliness_ms_;
    int health_status_timeout_ms_;
    int obstacle_timeout_ms_;
    int velocity_timeout_ms_;


    bool has_health_status_{false};




    bool has_nearest_obstacle_{false};
    bool has_velocity_{false};
    float nearest_obstacle_m_{0.0F};
    float speed_mps_{0.0F};


    rclcpp::QoS heartbeat_qos_{rclcpp::KeepLast(10)};
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr heartbeat_publisher_;
    rclcpp::TimerBase::SharedPtr heartbeat_timer_;
    rclcpp::Time last_health_status_time_{0, 0, RCL_ROS_TIME};
    rclcpp::Time last_obstacle_time_{0, 0, RCL_ROS_TIME};
    rclcpp::Time last_velocity_time_{0, 0, RCL_ROS_TIME};




    std::unordered_map<std::string, TopicHealth> topic_health_;

    rclcpp::Subscription<std_msgs::msg::Float32>::SharedPtr
      nearest_obstacle_subscription_;
    rclcpp::Subscription<geometry_msgs::msg::TwistStamped>::SharedPtr
      velocity_subscription_;
    rclcpp::Subscription<HealthStatus>::SharedPtr health_subscription_;

    rclcpp::Publisher<SafetyStatus>::SharedPtr safety_publisher_;
    rclcpp::TimerBase::SharedPtr evaluation_timer_;
  };

  int main(int argc, char ** argv)
  {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<SafetyFusionNode>());
    rclcpp::shutdown();
    return 0;
  }



