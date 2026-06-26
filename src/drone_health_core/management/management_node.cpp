  #include <chrono>
  #include <stdexcept>
  #include <string>
  #include <unordered_map>
  #include <vector>

  #include "drone_health_interfaces/msg/management_state.hpp"
  #include "drone_health_interfaces/srv/set_module_inactive.hpp"
  #include "drone_health_interfaces/srv/register_module.hpp"
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
  using SupervisorStatus = drone_health_interfaces::msg::SupervisorStatus;
  using SetBool = std_srvs::srv::SetBool;

  struct TopicConfig
  {
    std::string name;
    std::string type;
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
      response->success = false;
      response->message = "module_name must not be empty";
      return;
    }

    if (request->heartbeat_topic.empty()) {
      response->success = false;
      response->message = "heartbeat_topic must not be empty";
      return;
    }

    if (request->heartbeat_type.empty()) {
      response->success = false;
      response->message = "heartbeat_type must not be empty";
      return;
    }

    if (request->heartbeat_topic.front() != '/') {
      response->success = false;
      response->message = "heartbeat_topic must start with /";
      return;
    }


    if (request->heartbeat_deadline_ms < 0 ||
        request->heartbeat_liveliness_ms < 0)
    {
      response->success = false;
      response->message = "heartbeat deadline/liveliness must not be negative";
      return;
    }

    if (request->heartbeat_deadline_ms == 0 &&
        request->heartbeat_liveliness_ms == 0)
    {
      response->success = false;
      response->message =
        "heartbeat must provide deadline_ms or liveliness_ms";
      return;
    }

    if (request->heartbeat_deadline_ms > 0 &&
        request->heartbeat_liveliness_ms > 0 &&
        request->heartbeat_liveliness_ms <= request->heartbeat_deadline_ms)
    {
      response->success = false;
      response->message =
        "heartbeat liveliness_ms must be greater than deadline_ms when both are set";
      return;
    }


    if (request->data_topics.size() != request->data_topic_types.size()) {
      response->success = false;
      response->message = "data_topics and data_topic_types size must match";
      return;
    }



    ModuleConfig config;
    config.critical = request->critical;

    TopicConfig heartbeat;
    heartbeat.name = request->heartbeat_topic;
    heartbeat.type = request->heartbeat_type;
    heartbeat.is_heartbeat = true;
    heartbeat.deadline_ms = request->heartbeat_deadline_ms;
    heartbeat.liveliness_ms = request->heartbeat_liveliness_ms;

    config.topics.push_back(heartbeat.name);
    config.topic_configs.push_back(heartbeat);

    for (size_t i = 0; i < request->data_topics.size(); ++i) {
      if (request->data_topics[i].empty() ||
      request->data_topic_types[i].empty()) {
        response->success = false;
        response->message = "data topic names and types must not be empty";
        return;
      }

      if (request->data_topics[i].front() != '/') {
        response->success = false;
        response->message = "data topic names must start with /";
        return;
      }



      TopicConfig data_topic;
      data_topic.name = request->data_topics[i];
      data_topic.type = request->data_topic_types[i];
      data_topic.is_heartbeat = false;
      data_topic.deadline_ms = 0;
      data_topic.liveliness_ms = 0;

      config.topics.push_back(data_topic.name);
      config.topic_configs.push_back(data_topic);
    }

    modules_[request->module_name] = config;
    planned_inactive_modules_.erase(request->module_name);

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



//That section publishes runtime-registered topic metadata into /management/state.
  // Publish runtime-registered topic metadata for HealthMonitor and dashboard.
  // YAML modules use the simple module->topics list; runtime modules also fill topic_configs.

      for (const auto & module_item : modules_) {
        const auto & module_name = module_item.first;
        const auto & module = module_item.second;

        state.registry_modules.push_back(module_name);

        if (planned_inactive_modules_.find(module_name) ==
            planned_inactive_modules_.end())
        {
          state.active_modules.push_back(module_name);
        }

        for (const auto & topic : module.topic_configs) {
          state.registry_topic_modules.push_back(module_name);
          state.registry_topics.push_back(topic.name);
          state.registry_topic_types.push_back(topic.type);
          state.registry_topic_critical.push_back(module.critical);
          state.registry_topic_is_heartbeat.push_back(topic.is_heartbeat);
          state.registry_topic_deadline_ms.push_back(topic.deadline_ms);
          state.registry_topic_liveliness_ms.push_back(topic.liveliness_ms);
        }
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
