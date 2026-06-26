#include <chrono>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>

#include "geometry_msgs/msg/twist_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

class SimulatedFlowSensorNode : public rclcpp::Node
{
public:
  SimulatedFlowSensorNode()
  : Node("simulated_flow_sensor_node")
  {
    declare_parameters();
    read_parameters();
    setup_qos();
    setup_publishers();
    setup_timer();

    RCLCPP_INFO(get_logger(), "Simulated flow sensor started");
  }

private:
  void declare_parameters()
  {
    declare_parameter<std::string>("frame_id", "base_link");
    declare_parameter<int>("publish_period_ms", 100);
    declare_parameter<int>("velocity_deadline_ms", 200);
    declare_parameter<int>("heartbeat_deadline_ms", 300);
    declare_parameter<int>("heartbeat_liveliness_ms", 1000);
    declare_parameter<double>("base_forward_velocity_mps", 1.0);
    declare_parameter<double>("side_drift_amplitude_mps", 0.2);
    declare_parameter<bool>("simulate_motion", false);
    declare_parameter<double>("stationary_speed_mps", 0.0);


  }

  void read_parameters()
  {
    frame_id_ = get_parameter("frame_id").as_string();
    publish_period_ms_ = get_parameter("publish_period_ms").as_int();
    velocity_deadline_ms_ = get_parameter("velocity_deadline_ms").as_int();
    heartbeat_deadline_ms_ = get_parameter("heartbeat_deadline_ms").as_int();
    heartbeat_liveliness_ms_ = get_parameter("heartbeat_liveliness_ms").as_int();
    base_forward_velocity_mps_ = get_parameter("base_forward_velocity_mps").as_double();
    side_drift_amplitude_mps_ = get_parameter("side_drift_amplitude_mps").as_double();
    simulate_motion_ = get_parameter("simulate_motion").as_bool();
    stationary_speed_mps_ = get_parameter("stationary_speed_mps").as_double();


    if (publish_period_ms_ <= 0) {
      throw std::runtime_error("publish_period_ms must be greater than 0");
    }

    if (velocity_deadline_ms_ <= publish_period_ms_) {
      throw std::runtime_error("velocity_deadline_ms must be greater than publish_period_ms");
    }

    if (heartbeat_deadline_ms_ <= 0 || heartbeat_liveliness_ms_ <= heartbeat_deadline_ms_) {
      throw std::runtime_error("heartbeat liveliness must be greater than heartbeat deadline");
    }
  }

  void setup_qos()
  {
    velocity_qos_ = rclcpp::QoS(rclcpp::KeepLast(5))
      .best_effort()
      .deadline(std::chrono::milliseconds(velocity_deadline_ms_));

    heartbeat_qos_ = rclcpp::QoS(rclcpp::KeepLast(10))
      .reliable()
      .deadline(std::chrono::milliseconds(heartbeat_deadline_ms_))
      .liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC)
      .liveliness_lease_duration(std::chrono::milliseconds(heartbeat_liveliness_ms_));
  }

  void setup_publishers()
  {
    velocity_publisher_ = create_publisher<geometry_msgs::msg::TwistStamped>(
      "/vehicle/velocity", velocity_qos_);

    heartbeat_publisher_ = create_publisher<std_msgs::msg::String>(
      "/flow/heartbeat", heartbeat_qos_);
  }

  void setup_timer()
  {
    timer_ = create_wall_timer(
      std::chrono::milliseconds(publish_period_ms_),
      std::bind(&SimulatedFlowSensorNode::timer_callback, this));
  }

  void timer_callback()
  {
    simulation_time_s_ += static_cast<double>(publish_period_ms_) / 1000.0;

    publish_velocity();
    publish_heartbeat();

    if (!heartbeat_publisher_->assert_liveliness()) {
      RCLCPP_WARN(get_logger(), "Failed to assert flow sensor liveliness");
    }
  }

  void publish_velocity()
  {
    geometry_msgs::msg::TwistStamped velocity;
    velocity.header.stamp = now();
    velocity.header.frame_id = frame_id_;


    if (simulate_motion_) {
      velocity.twist.linear.x =
        base_forward_velocity_mps_ + 0.3 * std::sin(simulation_time_s_ * 0.5);
      velocity.twist.linear.y =
        side_drift_amplitude_mps_ * std::sin(simulation_time_s_);
    } else {
      velocity.twist.linear.x = stationary_forward_velocity_mps_;
      velocity.twist.linear.y = 0.0;
    }
    velocity.twist.linear.z = 0.0;





    velocity.twist.angular.x = 0.0;
    velocity.twist.angular.y = 0.0;
    velocity.twist.angular.z = 0.0;

    velocity_publisher_->publish(velocity);
  }

  void publish_heartbeat()
  {
    std_msgs::msg::String heartbeat;
    heartbeat.data = "simulated_flow_sensor_node alive";
    heartbeat_publisher_->publish(heartbeat);
  }

  std::string frame_id_;
  int publish_period_ms_;
  int velocity_deadline_ms_;
  int heartbeat_deadline_ms_;
  int heartbeat_liveliness_ms_;
  bool simulate_motion_{false};
  double stationary_speed_mps_{0.0};
  double stationary_forward_velocity_mps_{0.0};

  double base_forward_velocity_mps_;
  double side_drift_amplitude_mps_;
  double simulation_time_s_{0.0};


  rclcpp::QoS velocity_qos_{rclcpp::KeepLast(5)};
  rclcpp::QoS heartbeat_qos_{rclcpp::KeepLast(10)};
  rclcpp::Publisher<geometry_msgs::msg::TwistStamped>::SharedPtr velocity_publisher_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr heartbeat_publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<SimulatedFlowSensorNode>());
  rclcpp::shutdown();
  return 0;
}
