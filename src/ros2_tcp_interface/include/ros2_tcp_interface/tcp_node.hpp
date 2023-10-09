//
// Created by ryuzo on 2022/08/08.
//

#ifndef ROS2_MASTER_SOCKETCAN_TX_NODE_HPP
#define ROS2_MASTER_SOCKETCAN_TX_NODE_HPP

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>
#include <vector>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/eventfd.h>

#include "tcp_interface/msg/tcp_socket.hpp"
#include "tcp_interface/srv/tcp_socket_i_ctrl.hpp"
#include "ros2_tcp_interface/visibility_control.h"

using LNI = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface;

namespace tcp_interface {

    class TcpInterface final : public rclcpp_lifecycle::LifecycleNode {
    private:
        rclcpp::Service<tcp_interface::srv::TcpSocketICtrl>::SharedPtr srv_;
        void handleService_(
                const std::shared_ptr<tcp_interface::srv::TcpSocketICtrl::Request>& request,
                const std::shared_ptr<tcp_interface::srv::TcpSocketICtrl::Response>& response
        );
        rclcpp::Subscription<tcp_interface::msg::TcpSocket>::SharedPtr _subscription;
        rclcpp::TimerBase::SharedPtr _pub_timer;

        int64_t interval_ms;

        std::map<uint16_t, std::unique_ptr<std::thread>> tcp_port_threads;

        rclcpp::QoS _qos = rclcpp::QoS(40);

        void _publisher_callback();
        void _subscriber_callback(tcp_interface::msg::TcpSocket msg);

        void server_thread(int port, pthread_t parent_pthread_t);
        std::recursive_mutex thread_map_mtx_;
        void detach_thread(int port);

        int newconnection_eventfd;

        std::recursive_mutex current_state_mtx_;
        rclcpp_lifecycle::State _get_current_state();
    public:
        
        ROS2_TCP_INTERFACE_PUBLIC
        explicit TcpInterface(const rclcpp::NodeOptions& options = rclcpp::NodeOptions());

        LNI::CallbackReturn on_configure(const rclcpp_lifecycle::State & state) override;

        LNI::CallbackReturn on_activate(const rclcpp_lifecycle::State & state) override;

        LNI::CallbackReturn on_deactivate(const rclcpp_lifecycle::State & state) override;

        LNI::CallbackReturn on_cleanup(const rclcpp_lifecycle::State & state) override;

        LNI::CallbackReturn on_shutdown(const rclcpp_lifecycle::State & state) override;
    };
}

#endif //ROS2_MASTER_SOCKETCAN_TX_NODE_HPP
