#include <algorithm>
  #include <chrono>
  #include <cmath>
  #include <memory>
  #include <string>
  #include <vector>

  #include "rclcpp/rclcpp.hpp"
  #include "sensor_msgs/msg/laser_scan.hpp"
  #include "std_msgs/msg/string.hpp"

  class SimulatedLidarDriverNode : public rclcpp::Node
  {
  public:
    SimulatedLidarDriverNode()
    : Node("simulated_lidar_driver_node")
    {
      declare_parameters();
      read_parameters();
      setup_qos();
      setup_publishers();
      setup_timer();

      RCLCPP_INFO(get_logger(), "Simulated LiDAR driver started");
    }

  private:
    void declare_parameters()
    {
      declare_parameter<std::string>("frame_id", "lidar_link");

      declare_parameter<int>("publish_period_ms", 100);

      declare_parameter<int>("scan_deadline_ms", 200);


      declare_parameter<int>("heartbeat_deadline_ms", 300);
      declare_parameter<int>("heartbeat_liveliness_ms", 1000);

      declare_parameter<double>("range_min_m", 0.12);
      declare_parameter<double>("range_max_m", 12.0);

      declare_parameter<double>("angle_min_rad", -1.5708);
      declare_parameter<double>("angle_max_rad", 1.5708);
      declare_parameter<int>("beam_count", 181);
    }

    void read_parameters()
    {
      frame_id_ = get_parameter("frame_id").as_string();

      publish_period_ms_ = get_parameter("publish_period_ms").as_int();

      scan_deadline_ms_ = get_parameter("scan_deadline_ms").as_int();


      heartbeat_deadline_ms_ = get_parameter("heartbeat_deadline_ms").as_int();
      heartbeat_liveliness_ms_ = get_parameter("heartbeat_liveliness_ms").as_int();

      range_min_m_ = get_parameter("range_min_m").as_double();
      range_max_m_ = get_parameter("range_max_m").as_double();

      angle_min_rad_ = get_parameter("angle_min_rad").as_double();
      angle_max_rad_ = get_parameter("angle_max_rad").as_double();
      beam_count_ = get_parameter("beam_count").as_int();

      if (publish_period_ms_ <= 0) {
        throw std::runtime_error("publish_period_ms must be greater than 0");
      }

      if (beam_count_ < 2) {
        throw std::runtime_error("beam_count must be at least 2");
      }

      if (range_min_m_ <= 0.0 || range_max_m_ <= range_min_m_) {
        throw std::runtime_error("range limits are invalid");
      }

      if (angle_max_rad_ <= angle_min_rad_) {
        throw std::runtime_error("angle limits are invalid");
      }
    }

    void setup_qos()
    {
      scan_qos_ = rclcpp::QoS(rclcpp::KeepLast(5))
        .best_effort()
        .deadline(std::chrono::milliseconds(scan_deadline_ms_));

      heartbeat_qos_ = rclcpp::QoS(rclcpp::KeepLast(10))
        .reliable()
        .deadline(std::chrono::milliseconds(heartbeat_deadline_ms_))
        .liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC)
        .liveliness_lease_duration(std::chrono::milliseconds(heartbeat_liveliness_ms_));
    }

    void setup_publishers()
    {
      scan_publisher_ = create_publisher<sensor_msgs::msg::LaserScan>(
        "/lidar/scan",
        scan_qos_);

      heartbeat_publisher_ = create_publisher<std_msgs::msg::String>(
        "/lidar/heartbeat",
        heartbeat_qos_);
    }

    void setup_timer()
    {
      timer_ = create_wall_timer(
        std::chrono::milliseconds(publish_period_ms_),
        std::bind(&SimulatedLidarDriverNode::timer_callback, this));
    }

    void timer_callback()
    {
      simulation_time_s_ += static_cast<double>(publish_period_ms_) / 1000.0;

      publish_scan();
      publish_heartbeat();


      heartbeat_publisher_->assert_liveliness();
    }

    void publish_scan()
    {
      sensor_msgs::msg::LaserScan scan;

      scan.header.stamp = now();
      scan.header.frame_id = frame_id_;

      scan.angle_min = static_cast<float>(angle_min_rad_);
      scan.angle_max = static_cast<float>(angle_max_rad_);
      scan.angle_increment = static_cast<float>(
        (angle_max_rad_ - angle_min_rad_) / static_cast<double>(beam_count_ - 1));

      scan.time_increment = 0.0F;
      scan.scan_time = static_cast<float>(publish_period_ms_) / 1000.0F;

      scan.range_min = static_cast<float>(range_min_m_);
      scan.range_max = static_cast<float>(range_max_m_);

      scan.ranges = generate_ranges();
      scan.intensities.resize(scan.ranges.size(), 100.0F);

      scan_publisher_->publish(scan);
    }

    std::vector<float> generate_ranges()
    {
      std::vector<float> ranges;
      ranges.reserve(static_cast<size_t>(beam_count_));

      const float front_obstacle_distance = static_cast<float>(
        2.5 + 0.7 * std::sin(simulation_time_s_));

      const float right_obstacle_distance = static_cast<float>(
        4.0 + 0.5 * std::sin(simulation_time_s_ * 0.6));

      const float left_obstacle_distance = static_cast<float>(
        5.0 + 0.4 * std::cos(simulation_time_s_ * 0.4));

      for (int i = 0; i < beam_count_; ++i) {
        const double ratio = static_cast<double>(i) / static_cast<double>(beam_count_ - 1);
        const double angle = angle_min_rad_ + ratio * (angle_max_rad_ - angle_min_rad_);

        float distance = static_cast<float>(range_max_m_);

        if (std::abs(angle) < 0.25) {
          distance = front_obstacle_distance;
        } else if (angle > 0.55 && angle < 0.85) {
          distance = right_obstacle_distance;
        } else if (angle < -0.75 && angle > -1.05) {
          distance = left_obstacle_distance;
        }

        distance = std::clamp(
          distance,
          static_cast<float>(range_min_m_),
          static_cast<float>(range_max_m_));

        ranges.push_back(distance);
      }

      return ranges;
    }

    void publish_heartbeat()
    {
      std_msgs::msg::String heartbeat;
      heartbeat.data = "simulated_lidar_driver_node alive";
      heartbeat_publisher_->publish(heartbeat);
    }

    std::string frame_id_;

    int publish_period_ms_;
    int scan_deadline_ms_;

    int heartbeat_deadline_ms_;
    int heartbeat_liveliness_ms_;

    double range_min_m_;
    double range_max_m_;
    double angle_min_rad_;
    double angle_max_rad_;
    int beam_count_;

    double simulation_time_s_{0.0};

    rclcpp::QoS scan_qos_{rclcpp::KeepLast(5)};
    rclcpp::QoS heartbeat_qos_{rclcpp::KeepLast(10)};

    rclcpp::Publisher<sensor_msgs::msg::LaserScan>::SharedPtr scan_publisher_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr heartbeat_publisher_;
    rclcpp::TimerBase::SharedPtr timer_;
  };

  int main(int argc, char ** argv)
  {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<SimulatedLidarDriverNode>());
    rclcpp::shutdown();
    return 0;
  }


