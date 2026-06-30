#include <chrono>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "drone_health_interfaces/msg/management_state.hpp"
#include "drone_health_interfaces/srv/set_module_inactive.hpp"
#include "drone_health_interfaces/srv/register_module.hpp"
#include "drone_health_interfaces/msg/managed_module.hpp"
#include "drone_health_interfaces/msg/monitor_spec.hpp"
#include "drone_health_interfaces/srv/deregister_module.hpp"
#include "drone_health_interfaces/msg/supervisor_status.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_srvs/srv/set_bool.hpp"
#include "std_msgs/msg/string.hpp"

class ManagementNode : public rclcpp::Node
{
public:
  ManagementNode()
  : Node("management_node")
  {
    declare_module_parameters();
    read_module_parameters();
    state_publisher_ = create_publisher<ManagementState>(
        "/management/state",
        rclcpp::QoS(rclcpp::KeepLast(10)).reliable());

    auto heartbeat_qos = rclcpp::QoS(rclcpp::KeepLast(10))
      .reliable()
      .deadline(std::chrono::milliseconds(700))
      .liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC)
      .liveliness_lease_duration(std::chrono::milliseconds(1500));

    heartbeat_publisher_ = create_publisher<std_msgs::msg::String>(
        "/management/heartbeat",
        heartbeat_qos);


    supervisor_subscription_ = create_subscription<SupervisorStatus>(
      supervisor_status_topic_,
      rclcpp::QoS(rclcpp::KeepLast(10)).reliable(),
      std::bind(
        &ManagementNode::handle_supervisor_status,
        this,
        std::placeholders::_1));

    maintenance_service_ = create_service<SetBool>(
        "/management/set_maintenance_mode",
        std::bind(
          &ManagementNode::handle_set_maintenance_mode,
          this,
          std::placeholders::_1,
          std::placeholders::_2));

    mission_service_ = create_service<SetBool>(
        "/management/set_mission_active",
        std::bind(
          &ManagementNode::handle_set_mission_active,
          this,
          std::placeholders::_1,
          std::placeholders::_2));

    module_inactive_service_ = create_service<SetModuleInactive>(
        "/management/set_module_inactive",
        std::bind(
          &ManagementNode::handle_set_module_inactive,
          this,
          std::placeholders::_1,
          std::placeholders::_2));
    register_module_service_ = create_service<RegisterModule>(
      "/management/register_module",
      std::bind(
        &ManagementNode::handle_register_module,
        this,
        std::placeholders::_1,
        std::placeholders::_2));

    deregister_module_service_ = create_service<DeregisterModule>(
      "/management/deregister_module",
      std::bind(
        &ManagementNode::handle_deregister_module,
        this,
        std::placeholders::_1,
        std::placeholders::_2));

    timer_ = create_wall_timer(
        std::chrono::milliseconds(200),
        std::bind(&ManagementNode::publish_state, this));

    RCLCPP_INFO(get_logger(), "Management node started");
  }

private:
  using ManagementState = drone_health_interfaces::msg::ManagementState;
  using SetModuleInactive = drone_health_interfaces::srv::SetModuleInactive;
  using RegisterModule = drone_health_interfaces::srv::RegisterModule;
  using DeregisterModule = drone_health_interfaces::srv::DeregisterModule;
  using ManagedModule = drone_health_interfaces::msg::ManagedModule;
  using MonitorSpec = drone_health_interfaces::msg::MonitorSpec;
  using SupervisorStatus = drone_health_interfaces::msg::SupervisorStatus;
  using SetBool = std_srvs::srv::SetBool;

  struct TopicConfig
  {
    std::string name;
    std::string type;
    std::string reliability{"reliable"};
    bool is_heartbeat{false};
    int deadline_ms{0};
    int liveliness_ms{0};
  };

  struct ModuleConfig
  {
    bool critical{false};
    std::vector<std::string> topics;
    std::vector<TopicConfig> topic_configs;
  };

  void handle_set_maintenance_mode(
    const SetBool::Request::SharedPtr request,
    SetBool::Response::SharedPtr response)
  {
    if (request->data && mission_active_) {
      response->success = false;
      response->message = "cannot enable maintenance mode during active mission";
      return;
    }

    maintenance_mode_ = request->data;
    response->success = true;

    if (maintenance_mode_) {
      response->message = "maintenance mode enabled";
    } else {
      response->message = "maintenance mode disabled";
    }

    publish_state();
  }

  void handle_set_mission_active(
    const SetBool::Request::SharedPtr request,
    SetBool::Response::SharedPtr response)
  {
    if (request->data) {
      if (maintenance_mode_) {
        response->success = false;
        response->message = "cannot start mission while maintenance mode is active";
        return;
      }

      if (has_planned_inactive_critical_item()) {
        response->success = false;
        response->message =
          "cannot start mission while critical module/topic is planned inactive";
        return;
      }

      if (!supervisor_allows_mission_start()) {
        response->success = false;
        response->message =
          "cannot start mission because supervisor does not allow command";
        return;
      }
    }

    mission_active_ = request->data;
    response->success = true;

    if (mission_active_) {
      response->message = "mission active enabled";
    } else {
      response->message = "mission active disabled";
    }

    publish_state();
  }

  void handle_set_module_inactive(
    const SetModuleInactive::Request::SharedPtr request,
    SetModuleInactive::Response::SharedPtr response)
  {
    if (request->module_name.empty()) {
      response->success = false;
      response->message = "module_name must not be empty";
      return;
    }

    if (!module_name_valid(request->module_name)) {
      response->success = false;
      response->message = "module_name is not configured: " + request->module_name;
      return;
    }

    if (request->inactive) {
      if (!inactive_reason_valid(request->reason)) {
        response->success = false;
        response->message =
          "reason must be maintenance, deregistered, optional_disabled, or mission_not_required";
        return;
      }
      if (mission_active_ && module_is_critical(request->module_name)) {
        response->success = false;
        response->message =
          "cannot mark critical module inactive during active mission: " +
          request->module_name;
        return;
      }

      planned_inactive_modules_[request->module_name] = request->reason;
      response->success = true;
      response->message =
        "module marked planned inactive: " + request->module_name + " - " +
        request->reason;
      publish_state();
      return;
    }

    const auto removed = planned_inactive_modules_.erase(request->module_name);
    response->success = true;

    if (removed > 0) {
      response->message = "module restored active: " + request->module_name;
    } else {
      response->message = "module was not planned inactive: " + request->module_name;
    }

    publish_state();
  }

  void handle_register_module(
    const RegisterModule::Request::SharedPtr request,
    RegisterModule::Response::SharedPtr response)
  {
    if (request->module_name.empty()) {
      reject_register_module(
        response,
        request->module_name,
        "module_name must not be empty");
      return;
    }

    if (!request->monitors.empty()) {
      ModuleConfig config;
      config.critical = request->critical;

      int heartbeat_count = 0;

      for (const auto & monitor : request->monitors) {
        if (monitor.topic_name.empty() || monitor.topic_name.front() != '/') {
          reject_register_module(
            response,
            request->module_name,
            "monitor topic names must start with /");
          return;
        }

        if (topic_already_registered_to_other_module(
            request->module_name,
            monitor.topic_name))
        {
          reject_register_module(
            response,
            request->module_name,
            "monitor topic already registered: " + monitor.topic_name);
          return;
        }

        if (monitor.kind != "heartbeat" && monitor.kind != "data") {
          reject_register_module(
            response,
            request->module_name,
            "monitor kind must be heartbeat or data");
          return;
        }

        if (monitor.message_type.empty()) {
          reject_register_module(
            response,
            request->module_name,
            "monitor message_type must not be empty");
          return;
        }

        if (monitor.reliability != "reliable" &&
          monitor.reliability != "best_effort")
        {
          reject_register_module(
            response,
            request->module_name,
            "monitor reliability must be reliable or best_effort");
          return;
        }

        if (monitor.deadline_ms < 0 || monitor.liveliness_ms < 0) {
          reject_register_module(
            response,
            request->module_name,
            "monitor deadline/liveliness must not be negative");
          return;
        }

        if (monitor.kind == "heartbeat") {
          ++heartbeat_count;

          if (monitor.deadline_ms == 0 && monitor.liveliness_ms == 0) {
            reject_register_module(
              response,
              request->module_name,
              "heartbeat monitor must provide deadline_ms or liveliness_ms");
            return;
          }
        }

        TopicConfig topic;
        topic.name = monitor.topic_name;
        topic.type = monitor.message_type;
        topic.reliability = monitor.reliability;
        topic.is_heartbeat = monitor.kind == "heartbeat";
        topic.deadline_ms = monitor.deadline_ms;
        topic.liveliness_ms = monitor.liveliness_ms;

        config.topics.push_back(topic.name);
        config.topic_configs.push_back(topic);
      }

      if (heartbeat_count != 1) {
        reject_register_module(
          response,
          request->module_name,
          "exactly one heartbeat monitor is required");
        return;
      }

      modules_[request->module_name] = config;
      planned_inactive_modules_.erase(request->module_name);
      rejected_modules_.erase(request->module_name);

      response->success = true;
      response->message = "module registered: " + request->module_name;

      publish_state();
      return;
    }

    if (request->heartbeat_topic.empty()) {
      reject_register_module(
        response,
        request->module_name,
        "heartbeat_topic must not be empty");
      return;
    }

    if (request->heartbeat_type.empty()) {
      reject_register_module(
        response,
        request->module_name,
        "heartbeat_type must not be empty");
      return;
    }

    if (request->heartbeat_topic.front() != '/') {
      reject_register_module(
        response,
        request->module_name,
        "heartbeat_topic must start with /");
      return;
    }

    if (topic_already_registered_to_other_module(
        request->module_name,
        request->heartbeat_topic))
    {
      reject_register_module(
        response,
        request->module_name,
        "heartbeat topic already registered: " + request->heartbeat_topic);
      return;
    }

    if (request->heartbeat_deadline_ms < 0 ||
        request->heartbeat_liveliness_ms < 0)
    {
      reject_register_module(
        response,
        request->module_name,
        "heartbeat deadline/liveliness must not be negative");
      return;
    }

    if (request->heartbeat_deadline_ms == 0 &&
        request->heartbeat_liveliness_ms == 0)
    {
      reject_register_module(
        response,
        request->module_name,
        "heartbeat must provide deadline_ms or liveliness_ms");
      return;
    }

    if (request->heartbeat_deadline_ms > 0 &&
        request->heartbeat_liveliness_ms > 0 &&
        request->heartbeat_liveliness_ms <= request->heartbeat_deadline_ms)
    {
      reject_register_module(
        response,
        request->module_name,
        "heartbeat liveliness_ms must be greater than deadline_ms when both are set");
      return;
    }

    if (request->data_topics.size() != request->data_topic_types.size()) {
      reject_register_module(
        response,
        request->module_name,
        "data_topics and data_topic_types size must match");
      return;
    }

    ModuleConfig config;
    config.critical = request->critical;

    TopicConfig heartbeat;
    heartbeat.name = request->heartbeat_topic;
    heartbeat.type = request->heartbeat_type;
    heartbeat.reliability = "reliable";
    heartbeat.is_heartbeat = true;
    heartbeat.deadline_ms = request->heartbeat_deadline_ms;
    heartbeat.liveliness_ms = request->heartbeat_liveliness_ms;

    config.topics.push_back(heartbeat.name);
    config.topic_configs.push_back(heartbeat);

    for (size_t i = 0; i < request->data_topics.size(); ++i) {
      if (request->data_topics[i].empty() ||
        request->data_topic_types[i].empty())
      {
        reject_register_module(
          response,
          request->module_name,
          "data topic names and types must not be empty");
        return;
      }

      if (request->data_topics[i].front() != '/') {
        reject_register_module(
          response,
          request->module_name,
          "data topic names must start with /");
        return;
      }

      if (topic_already_registered_to_other_module(
          request->module_name,
          request->data_topics[i]))
      {
        reject_register_module(
          response,
          request->module_name,
          "data topic already registered: " + request->data_topics[i]);
        return;
      }

      TopicConfig data_topic;
      data_topic.name = request->data_topics[i];
      data_topic.type = request->data_topic_types[i];
      data_topic.reliability = "reliable";
      data_topic.is_heartbeat = false;
      data_topic.deadline_ms = 0;
      data_topic.liveliness_ms = 0;

      config.topics.push_back(data_topic.name);
      config.topic_configs.push_back(data_topic);
    }

    modules_[request->module_name] = config;
    planned_inactive_modules_.erase(request->module_name);
    rejected_modules_.erase(request->module_name);

    response->success = true;
    response->message = "module registered: " + request->module_name;

    publish_state();
  }

  void handle_deregister_module(
    const DeregisterModule::Request::SharedPtr request,
    DeregisterModule::Response::SharedPtr response)
  {
    if (request->module_name.empty()) {
      response->success = false;
      response->message = "module_name must not be empty";
      return;
    }

    if (!module_name_valid(request->module_name)) {
      response->success = false;
      response->message = "module_name is not registered: " +
      request->module_name;
      return;
    }

    if (!inactive_reason_valid(request->reason)) {
      response->success = false;
      response->message =
        "reason must be maintenance, deregistered, optional_disabled, or mission_not_required";
      return;
    }

    if (mission_active_ && module_is_critical(request->module_name)) {
      response->success = false;
      response->message =
        "cannot deregister critical module during active mission: " +
        request->module_name;
      return;
    }

    planned_inactive_modules_[request->module_name] = request->reason;

    response->success = true;
    response->message =
      "module deregistered: " + request->module_name + " - " + request->reason;

    publish_state();
  }

  bool inactive_reason_valid(const std::string & reason) const
  {
    return reason == "maintenance" ||
           reason == "deregistered" ||
           reason == "optional_disabled" ||
           reason == "mission_not_required";
  }

  void reject_register_module(
    const RegisterModule::Response::SharedPtr response,
    const std::string & module_name,
    const std::string & reason)
  {
    response->success = false;
    response->message = reason;

    if (!module_name.empty()) {
      rejected_modules_[module_name] = reason;
    }

    publish_state();
  }

  void declare_module_parameters()
  {
    declare_parameter<std::string>(
        "supervisor_status_topic",
        "/supervisor/status");

    declare_parameter<int>(
        "supervisor_status_timeout_ms",
        1000);

    declare_parameter<std::vector<std::string>>(
        "module_ids",
        std::vector<std::string>{});
  }

  void read_module_parameters()
  {
    supervisor_status_topic_ = get_parameter("supervisor_status_topic").as_string();
    supervisor_status_timeout_ms_ = get_parameter("supervisor_status_timeout_ms").as_int();

    if (supervisor_status_topic_.empty()) {
      throw std::runtime_error("supervisor_status_topic must not be empty");
    }

    if (supervisor_status_timeout_ms_ <= 0) {
      throw std::runtime_error("supervisor_status_timeout_ms must be greater than 0");
    }


    module_ids_ = get_parameter("module_ids").as_string_array();

    if (module_ids_.empty()) {
      throw std::runtime_error("module_ids must not be empty");
    }

    for (const auto & module_id : module_ids_) {
      declare_parameter<bool>(module_id + ".critical", false);
      declare_parameter<std::vector<std::string>>(
          module_id + ".topics",
          std::vector<std::string>{});

      ModuleConfig config;
      config.critical = get_parameter(module_id + ".critical").as_bool();
      config.topics = get_parameter(module_id + ".topics").as_string_array();

      if (config.topics.empty()) {
        throw std::runtime_error(
            "module " + module_id + " must have at least one topic");
      }

      modules_.emplace(module_id, config);
    }
  }

  bool module_name_valid(const std::string & module_name) const
  {
    return modules_.find(module_name) != modules_.end();
  }
  bool module_is_critical(const std::string & module_name) const
  {
    const auto module = modules_.find(module_name);
    return module != modules_.end() && module->second.critical;
  }

  bool topic_is_critical(const std::string & topic_name) const
  {
    for (const auto & item : modules_) {
      const auto & config = item.second;

      if (!config.critical) {
        continue;
      }

      for (const auto & topic : config.topics) {
        if (topic == topic_name) {
          return true;
        }
      }
    }

    return false;
  }

  bool topic_already_registered_to_other_module(
    const std::string & module_name,
    const std::string & topic_name) const
  {
    for (const auto & item : modules_) {
      if (item.first == module_name) {
        continue;
      }

      const auto & config = item.second;

      for (const auto & topic : config.topics) {
        if (topic == topic_name) {
          return true;
        }
      }
    }

    return false;
  }

  bool has_planned_inactive_critical_item() const
  {
    for (const auto & item : planned_inactive_modules_) {
      if (module_is_critical(item.first)) {
        return true;
      }
    }

    for (const auto & item : planned_inactive_topics_) {
      if (topic_is_critical(item.first)) {
        return true;
      }
    }

    return false;
  }

  void handle_supervisor_status(const SupervisorStatus::SharedPtr msg)
  {
    latest_supervisor_status_ = *msg;
    last_supervisor_status_time_ = now();
    has_supervisor_status_ = true;
  }

  bool supervisor_status_fresh()
  {
    if (!has_supervisor_status_) {
      return false;
    }

    const double age_s = (now() - last_supervisor_status_time_).seconds();
    const double timeout_s =
      static_cast<double>(supervisor_status_timeout_ms_) / 1000.0;

    return age_s <= timeout_s;
  }

  bool supervisor_allows_mission_start()
  {
    if (!supervisor_status_fresh()) {
      return false;
    }

    return latest_supervisor_status_.mode == SupervisorStatus::NORMAL &&
           latest_supervisor_status_.command_allowed;
  }

  void publish_state()
  {
    ManagementState state;
    state.header.stamp = now();
    state.header.frame_id = "base_link";
    state.maintenance_mode = maintenance_mode_;
    state.mission_active = mission_active_;

    for (const auto & item : planned_inactive_topics_) {
      state.planned_inactive_topics.push_back(item.first);
      state.planned_inactive_topic_reasons.push_back(item.second);
    }

    for (const auto & item : planned_inactive_modules_) {
      state.planned_inactive_modules.push_back(item.first);
      state.planned_inactive_module_reasons.push_back(item.second);

      const auto module = modules_.find(item.first);
      if (module == modules_.end()) {
        continue;
      }

      for (const auto & topic : module->second.topics) {
        bool already_added = false;

        for (const auto & existing_topic : state.planned_inactive_topics) {
          if (existing_topic == topic) {
            already_added = true;
            break;
          }
        }

        if (!already_added) {
          state.planned_inactive_topics.push_back(topic);
          state.planned_inactive_topic_reasons.push_back(item.second);
        }
      }
    }

    // Publish runtime-registered topic metadata for HealthMonitor and dashboard.
    // YAML modules use the simple module->topics list; runtime modules also fill topic_configs.
    for (const auto & module_item : modules_) {
      const auto & module_name = module_item.first;
      const auto & module = module_item.second;

      ManagedModule managed_module;
      managed_module.module_name = module_name;
      managed_module.critical = module.critical;

      if (planned_inactive_modules_.find(module_name) !=
        planned_inactive_modules_.end())
      {
        managed_module.state = "PLANNED_INACTIVE";
        managed_module.last_reason = planned_inactive_modules_.at(module_name);
      } else {
        managed_module.state = "REGISTERED";
        managed_module.last_reason = "registered";
      }

      for (const auto & topic : module.topic_configs) {
        MonitorSpec monitor;
        monitor.topic_name = topic.name;
        monitor.kind = topic.is_heartbeat ? "heartbeat" : "data";
        monitor.message_type = topic.type;
        monitor.reliability = topic.reliability;
        monitor.deadline_ms = topic.deadline_ms;
        monitor.liveliness_ms = topic.liveliness_ms;
        managed_module.monitors.push_back(monitor);
      }

      state.managed_modules.push_back(managed_module);
    }

    for (const auto & item : rejected_modules_) {
      state.rejected_modules.push_back(item.first);
      state.rejected_module_reasons.push_back(item.second);
    }

    state.reason = ManagementState::REASON_NONE;
    state.message = "management state normal";

    if (maintenance_mode_) {
      state.reason = ManagementState::REASON_MAINTENANCE_MODE;
      state.message = "maintenance mode active";
    } else if (!planned_inactive_modules_.empty()) {
      state.reason = ManagementState::REASON_PLANNED_INACTIVE;

      if (planned_inactive_modules_.size() == 1) {
        const auto & item = *planned_inactive_modules_.begin();
        state.message = "planned inactive module: " + item.first + " - " + item.second;
      } else {
        state.message =
          std::to_string(planned_inactive_modules_.size()) + " planned inactive modules";
      }
    } else if (!planned_inactive_topics_.empty()) {
      state.reason = ManagementState::REASON_PLANNED_INACTIVE;

      if (planned_inactive_topics_.size() == 1) {
        const auto & item = *planned_inactive_topics_.begin();
        state.message = "planned inactive: " + item.first + " - " + item.second;
      } else {
        state.message =
          std::to_string(planned_inactive_topics_.size()) + " planned inactive topics";
      }
    }

    std_msgs::msg::String heartbeat;
    heartbeat.data = "management_node alive";
    heartbeat_publisher_->publish(heartbeat);

    if (!heartbeat_publisher_->assert_liveliness()) {
      RCLCPP_WARN(get_logger(), "Failed to assert management node liveliness");
    }

    state_publisher_->publish(state);
  }

  bool maintenance_mode_{false};
  bool mission_active_{false};
  int supervisor_status_timeout_ms_;

  bool has_supervisor_status_{false};
  SupervisorStatus latest_supervisor_status_;

  rclcpp::Time last_supervisor_status_time_{0, 0, RCL_ROS_TIME};

  std::string supervisor_status_topic_;
  std::vector<std::string> module_ids_;
  std::unordered_map<std::string, ModuleConfig> modules_;

  std::unordered_map<std::string, std::string> planned_inactive_topics_;
  std::unordered_map<std::string, std::string> planned_inactive_modules_;
  std::unordered_map<std::string, std::string> rejected_modules_;

  rclcpp::Publisher<ManagementState>::SharedPtr state_publisher_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr heartbeat_publisher_;
  rclcpp::Subscription<SupervisorStatus>::SharedPtr supervisor_subscription_;
  rclcpp::Service<SetBool>::SharedPtr maintenance_service_;
  rclcpp::Service<SetBool>::SharedPtr mission_service_;
  rclcpp::Service<SetModuleInactive>::SharedPtr module_inactive_service_;
  rclcpp::Service<RegisterModule>::SharedPtr register_module_service_;
  rclcpp::Service<DeregisterModule>::SharedPtr deregister_module_service_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<ManagementNode>());
  rclcpp::shutdown();
  return 0;
}
