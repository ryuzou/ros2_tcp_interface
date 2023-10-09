cmake_minimum_required(VERSION 3.8)
project(tcp_interface)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake_auto REQUIRED)
ament_auto_find_build_dependencies()

set(msg_files
        "msg/TcpSocket.msg"
)
set(srv_files
        "srv/TcpSocketICtrl.srv"
)

rosidl_generate_interfaces(${PROJECT_NAME}
        ${msg_files}
        ${srv_files}
)

ament_auto_add_library(tcp_interface_component SHARED
        src/tcp_node.cpp)


rclcpp_components_register_node(tcp_interface_component
        PLUGIN "tcp_interface::TcpInterface"
        EXECUTABLE tcp_interface_node_exec)

rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")
target_link_libraries(tcp_interface_component "${cpp_typesupport_target}")

install(TARGETS
        tcp_interface_component
        DESTINATION lib/${PROJECT_NAME}
        LIBRARY DESTINATION lib)

install(
        DIRECTORY launch
        DESTINATION share/${PROJECT_NAME}
)


if(BUILD_TESTING)
    find_package(ament_lint_auto REQUIRED)
    # the following line skips the linter which checks for copyrights
    # uncomment the line when a copyright and license is not present in all source files
    #set(ament_cmake_copyright_FOUND TRUE)
    # the following line skips cpplint (only works in a git repo)
    # uncomment the line when this package is not in a git repo
    #set(ament_cmake_cpplint_FOUND TRUE)
    ament_lint_auto_find_test_dependencies()
endif()

ament_auto_package()
