cmake_minimum_required(VERSION 3.8)
  project(drone_health_core)

  if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
  endif()

  find_package(ament_cmake REQUIRED)
  find_package(rclcpp REQUIRED)
  find_package(std_msgs REQUIRED)
  find_package(sensor_msgs REQUIRED)
  find_package(geometry_msgs REQUIRED)
  find_package(std_srvs REQUIRED)
  find_package(drone_health_interfaces REQUIRED)



  add_executable(health_monitor_node
    health_monitor/health_monitor_node.cpp
  )
  ament_target_dependencies(health_monitor_node
    rclcpp
    std_msgs
    sensor_msgs
    geometry_msgs
    drone_health_interfaces
  )

  add_executable(supervisor_node
    supervisor/supervisor_node.cpp
  )
  ament_target_dependencies(supervisor_node
    rclcpp
    std_msgs
    drone_health_interfaces
    std_srvs
  )

  add_executable(management_node
    management/management_node.cpp
  )
  ament_target_dependencies(management_node
    rclcpp
    std_msgs
    std_srvs
    drone_health_interfaces
  )



  install(TARGETS

    health_monitor_node
    supervisor_node
    management_node
    DESTINATION lib/${PROJECT_NAME}
  )
  install(DIRECTORY
    health_monitor
    supervisor
    management
    DESTINATION share/${PROJECT_NAME}
    FILES_MATCHING
    PATTERN "*.yaml"
    PATTERN "*.md"
  )



  if(BUILD_TESTING)
    find_package(ament_lint_auto REQUIRED)
    set(ament_cmake_copyright_FOUND TRUE)
    set(ament_cmake_cpplint_FOUND TRUE)
    ament_lint_auto_find_test_dependencies()
  endif()

  ament_package()
