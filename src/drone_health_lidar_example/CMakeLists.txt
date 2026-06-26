
  cmake_minimum_required(VERSION 3.8)
  project(drone_health_lidar_example)

  if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
  endif()

  find_package(ament_cmake REQUIRED)
  find_package(rclcpp REQUIRED)
  find_package(std_msgs REQUIRED)
  find_package(sensor_msgs REQUIRED)

  add_executable(simulated_lidar_driver_node
    simulated_lidar_driver/simulated_lidar_driver_node.cpp
  )
  ament_target_dependencies(simulated_lidar_driver_node
    rclcpp
    std_msgs
    sensor_msgs
  )

  add_executable(lidar_obstacle_processor_node
    lidar_obstacle_processor/lidar_obstacle_processor_node.cpp
  )
  ament_target_dependencies(lidar_obstacle_processor_node
    rclcpp
    std_msgs
    sensor_msgs
  )

  install(TARGETS
    simulated_lidar_driver_node
    lidar_obstacle_processor_node
    DESTINATION lib/${PROJECT_NAME}
  )

  install(DIRECTORY
    simulated_lidar_driver
    lidar_obstacle_processor
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


