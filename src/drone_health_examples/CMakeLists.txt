  cmake_minimum_required(VERSION 3.8)
  project(drone_health_examples)

  if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
  endif()

  find_package(ament_cmake REQUIRED)
  find_package(rclcpp REQUIRED)
  find_package(std_msgs REQUIRED)
  find_package(std_srvs REQUIRED)
  find_package(sensor_msgs REQUIRED)
  find_package(drone_health_interfaces REQUIRED)

  add_executable(simulated_camera_node
    simulated_camera/simulated_camera_node.cpp
  )
  ament_target_dependencies(simulated_camera_node
    rclcpp
    std_msgs
    std_srvs
    sensor_msgs
    drone_health_interfaces
  )

  add_executable(mission_manager_node
    mission_manager/mission_manager_node.cpp
  )
  ament_target_dependencies(mission_manager_node
    rclcpp
    std_msgs
    std_srvs
  )

  install(TARGETS
    simulated_camera_node
    mission_manager_node
    DESTINATION lib/${PROJECT_NAME}
  )

  install(DIRECTORY
    simulated_camera
    mission_manager
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


