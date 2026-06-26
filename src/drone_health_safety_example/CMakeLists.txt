  cmake_minimum_required(VERSION 3.8)
  project(drone_health_safety_example)

  if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
  endif()

  find_package(ament_cmake REQUIRED)
  find_package(rclcpp REQUIRED)
  find_package(std_msgs REQUIRED)
  find_package(geometry_msgs REQUIRED)
  find_package(drone_health_interfaces REQUIRED)

  add_executable(safety_fusion_node
    safety_fusion/safety_fusion_node.cpp
  )
  ament_target_dependencies(safety_fusion_node
    rclcpp
    std_msgs
    geometry_msgs
    drone_health_interfaces
  )

  install(TARGETS
    safety_fusion_node
    DESTINATION lib/${PROJECT_NAME}
  )

  install(DIRECTORY
    safety_fusion
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

