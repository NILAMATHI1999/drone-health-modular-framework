cmake_minimum_required(VERSION 3.8)
project(drone_health_registrable_template)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(std_srvs REQUIRED)
find_package(drone_health_interfaces REQUIRED)

add_executable(registrable_publisher_template_node
  template_node/registrable_publisher_template_node.cpp
)
ament_target_dependencies(registrable_publisher_template_node
  rclcpp
  std_msgs
  std_srvs
  drone_health_interfaces
)

add_executable(registrable_subscriber_template_node
  template_node/registrable_subscriber_template_node.cpp
)
ament_target_dependencies(registrable_subscriber_template_node
  rclcpp
  std_msgs
  std_srvs
  drone_health_interfaces
)

add_executable(registrable_service_template_node
  template_node/registrable_service_template_node.cpp
)
ament_target_dependencies(registrable_service_template_node
  rclcpp
  std_msgs
  std_srvs
  drone_health_interfaces
)

add_executable(cpu_temperature_publisher
  template_node/cpu_temperature_publisher.cpp
)
ament_target_dependencies(cpu_temperature_publisher
  rclcpp
  std_msgs
  std_srvs
  drone_health_interfaces
)

install(TARGETS
  registrable_publisher_template_node
  registrable_subscriber_template_node
  registrable_service_template_node
  cpu_temperature_publisher
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY
  config
  DESTINATION share/${PROJECT_NAME}
)

install(DIRECTORY
  template_node
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
