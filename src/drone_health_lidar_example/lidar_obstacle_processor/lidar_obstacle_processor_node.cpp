#include <algorithm>
  #include <chrono>
  #include <cmath>
  #include <limits>
  #include <memory>
  #include <stdexcept>

  #include "rclcpp/rclcpp.hpp"
  #include "sensor_msgs/msg/laser_scan.hpp"
  #include "std_msgs/msg/float32.hpp"
  #include "std_msgs/msg/string.hpp"

  class LidarObstacleProcessorNode : public rclcpp::Node
  {
  public:
    LidarObstacleProcessorNode()
    : Node("lidar_obstacle_processor_node")
    {
      declare_parameters();
      read_parameters();
      setup_qos();
      setup_subscriber();
      setup_publisher();
      setup_heartbeat();

      RCLCPP_INFO(get_logger(), "LiDAR obstacle processor started");
    }

  private:
    void declare_parameters()
    {
      declare_parameter<double>("max_valid_range_m", 12.0);
      declare_parameter<int>("obstacle_deadline_ms", 250);
      declare_parameter<int>("heartbeat_period_ms", 500);
      declare_parameter<int>("heartbeat_deadline_ms", 700);
      declare_parameter<int>("heartbeat_liveliness_ms", 1500);
    }

    void read_parameters()
    {
      max_valid_range_m_ = get_parameter("max_valid_range_m").as_double();
      obstacle_deadline_ms_ = get_parameter("obstacle_deadline_ms").as_int();
      heartbeat_period_ms_ = get_parameter("heartbeat_period_ms").as_int();
      heartbeat_deadline_ms_ = get_parameter("heartbeat_deadline_ms").as_int();
      heartbeat_liveliness_ms_ = get_parameter("heartbeat_liveliness_ms").as_int();

      if (max_valid_range_m_ <= 0.0) {
        throw std::runtime_error("max_valid_range_m must be greater than 0");
      }

      if (obstacle_deadline_ms_ <= 0) {
        throw std::runtime_error("obstacle_deadline_ms must be greater than 0");
      }

      if (heartbeat_period_ms_ <= 0 || heartbeat_deadline_ms_ <= heartbeat_period_ms_) {
        throw std::runtime_error("heartbeat_deadline_ms must be greater than heartbeat_period_ms");
      }

      if (heartbeat_liveliness_ms_ <= heartbeat_deadline_ms_) {
        throw std::runtime_error("heartbeat_liveliness_ms must be greater than heartbeat_deadline_ms");
      }
    }

    void setup_qos()
    {
      scan_qos_ = rclcpp::QoS(rclcpp::KeepLast(5))
        .best_effort();

      obstacle_qos_ = rclcpp::QoS(rclcpp::KeepLast(10))
        .reliable()
        .deadline(std::chrono::milliseconds(obstacle_deadline_ms_));

      heartbeat_qos_ = rclcpp::QoS(rclcpp::KeepLast(10))
        .reliable()
        .deadline(std::chrono::milliseconds(heartbeat_deadline_ms_))
        .liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC)
        .liveliness_lease_duration(std::chrono::milliseconds(heartbeat_liveliness_ms_));
    }

    void setup_subscriber()
    {
      scan_subscription_ = create_subscription<sensor_msgs::msg::LaserScan>(
        "/lidar/scan",
        scan_qos_,
        std::bind(&LidarObstacleProcessorNode::scan_callback, this, std::placeholders::_1));
    }

    void setup_publisher()
    {
      nearest_obstacle_publisher_ = create_publisher<std_msgs::msg::Float32>("/lidar/nearest_obstacle",
        obstacle_qos_);
    }

    void setup_heartbeat()
    {
      heartbeat_publisher_ = create_publisher<std_msgs::msg::String>("/lidar_obstacle_processor/heartbeat",
        heartbeat_qos_);

      heartbeat_timer_ = create_wall_timer(std::chrono::milliseconds(heartbeat_period_ms_),        std::bind(&LidarObstacleProcessorNode::publish_heartbeat, this));
    }

    void scan_callback(const sensor_msgs::msg::LaserScan::SharedPtr scan)
    {
      const float nearest_distance = find_nearest_valid_range(*scan);

      std_msgs::msg::Float32 msg;
      msg.data = nearest_distance;

      nearest_obstacle_publisher_->publish(msg);
    }

    void publish_heartbeat()
    {
      std_msgs::msg::String heartbeat;
      heartbeat.data = "lidar_obstacle_processor_node alive";
      heartbeat_publisher_->publish(heartbeat);
      if (!heartbeat_publisher_->assert_liveliness()) {
        RCLCPP_WARN(get_logger(), "Failed to assert lidar obstacle processor liveliness");
      }
    }

    float find_nearest_valid_range(const sensor_msgs::msg::LaserScan & scan)
    const
    {
      float nearest = std::numeric_limits<float>::infinity();

      const float effective_max_range = std::min(scan.range_max,    static_cast<float>(max_valid_range_m_));

      for (const float range : scan.ranges) {
        const bool valid = std::isfinite(range) && range >= scan.range_min && range <= effective_max_range;

        if (valid && range < nearest) {
          nearest = range;
        }
      }

      if (!std::isfinite(nearest)) {
        return effective_max_range;
      }

      return nearest;
    }

    double max_valid_range_m_;
    int obstacle_deadline_ms_;
    int heartbeat_period_ms_;
    int heartbeat_deadline_ms_;
    int heartbeat_liveliness_ms_;

    rclcpp::QoS scan_qos_{rclcpp::KeepLast(5)};
    rclcpp::QoS obstacle_qos_{rclcpp::KeepLast(10)};
    rclcpp::QoS heartbeat_qos_{rclcpp::KeepLast(10)};

    rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr
    scan_subscription_;
    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr
    nearest_obstacle_publisher_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr heartbeat_publisher_;
    rclcpp::TimerBase::SharedPtr heartbeat_timer_;
  };

  int main(int argc, char ** argv)
  {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<LidarObstacleProcessorNode>());
    rclcpp::shutdown();
    return 0;
  }




