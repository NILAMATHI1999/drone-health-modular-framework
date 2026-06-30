cmake_minimum_required(VERSION 3.8)
project(drone_health_network_example)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

add_executable(wifi_monitor_node
  network_nodes/wifi_monitor_node.cpp
)
ament_target_dependencies(wifi_monitor_node
  rclcpp
  std_msgs
)

add_executable(lte_monitor_node
  network_nodes/lte_monitor_node.cpp
)
ament_target_dependencies(lte_monitor_node
  rclcpp
  std_msgs
)

add_executable(network_fusion_node
  network_nodes/network_fusion_node.cpp
)
ament_target_dependencies(network_fusion_node
  rclcpp
  std_msgs
)

add_executable(at_hilink_adapter_node
  network_nodes/at_hilink_adapter_node.cpp
)
ament_target_dependencies(at_hilink_adapter_node
  rclcpp
  std_msgs
)

add_executable(at_modem_monitor_node
  network_nodes/at_modem_monitor_node.cpp
)
ament_target_dependencies(at_modem_monitor_node
  rclcpp
  std_msgs
)

install(TARGETS
  wifi_monitor_node
  lte_monitor_node
  network_fusion_node
  at_hilink_adapter_node
  at_modem_monitor_node
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY
  network_nodes
  DESTINATION share/${PROJECT_NAME}
  FILES_MATCHING
  PATTERN "*.md"
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
