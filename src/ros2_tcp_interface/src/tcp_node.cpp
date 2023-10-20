//
// Created by ryuzo on 2022/08/08.
//
#include <rclcpp/rclcpp.hpp>
#include <lifecycle_msgs/msg/state.hpp>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <fcntl.h>
#include <mutex>
#include <csignal>
#include <pthread.h>
#include <cstdio>
#include <cstring>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>

#include "std_msgs/msg/string.hpp"
#include "ros2_tcp_interface/tcp_node.hpp"


#include <rclcpp_components/register_node_macro.hpp>
#include <arpa/inet.h>

namespace tcp_interface {

    TcpInterface::TcpInterface(const rclcpp::NodeOptions &options)
            : rclcpp_lifecycle::LifecycleNode("tcp_node", options) {
        using namespace std::chrono_literals;

        declare_parameter("interval_ms", 1);
        interval_ms = this->get_parameter("interval_ms").as_int();
        srv_ = this->create_service<tcp_interface::srv::TcpSocketICtrl>(
                "tcp_register",
                std::bind(&TcpInterface::handleService_, this, std::placeholders::_1, std::placeholders::_2)
        );
    }

    void TcpInterface::_publisher_callback() {
        if (this->_get_current_state().id() != lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE) {
            return;
        }
    }

    void TcpInterface::_subscriber_callback(const tcp_interface::msg::TcpSocket msg) {
        if (this->_get_current_state().id() != lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE) {
            return;
        }

    }

    LNI::CallbackReturn TcpInterface::on_configure(const rclcpp_lifecycle::State &state) {
        RCLCPP_INFO(this->get_logger(), "on config.");

        _pub_timer = this->create_wall_timer(
                std::chrono::milliseconds(interval_ms),
                [this] { _publisher_callback(); }
        );
        _subscription = this->create_subscription<tcp_interface::msg::TcpSocket>(
                "tcp_connection",
                _qos,
                std::bind(&TcpInterface::_subscriber_callback, this, std::placeholders::_1)
        );

        return LNI::CallbackReturn::SUCCESS;
    }

    LNI::CallbackReturn TcpInterface::on_activate(const rclcpp_lifecycle::State &state) {
        RCLCPP_INFO(this->get_logger(), "on activate.");
        return LNI::CallbackReturn::SUCCESS;
    }

    LNI::CallbackReturn TcpInterface::on_deactivate(const rclcpp_lifecycle::State &state) {
        RCLCPP_INFO(this->get_logger(), "on deactivate.");
        return LNI::CallbackReturn::SUCCESS;
    }

    LNI::CallbackReturn TcpInterface::on_cleanup(const rclcpp_lifecycle::State &state) {
        RCLCPP_INFO(this->get_logger(), "on cleanup.");
        _pub_timer->reset();
        _subscription.reset();
        return LNI::CallbackReturn::SUCCESS;
    }

    LNI::CallbackReturn TcpInterface::on_shutdown(const rclcpp_lifecycle::State &state) {
        RCLCPP_INFO(this->get_logger(), "on shutdown.");
        _pub_timer->reset();
        _subscription.reset();
        return LNI::CallbackReturn::SUCCESS;
    }

    rclcpp_lifecycle::State TcpInterface::_get_current_state() {
        std::lock_guard<std::recursive_mutex> lock(current_state_mtx_);
        return this->get_current_state();
    }

    void TcpInterface::server_thread(int port, pthread_t parent_pthread_t) {
        while (rclcpp::ok()) {
            if (this->_get_current_state().id() != lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE) {
                std::this_thread::sleep_for(std::chrono::microseconds(10));
                continue;
            }
            auto publisher = this->create_publisher<std_msgs::msg::String>(std::string("/tcp_" + std::to_string(port)), _qos);
            auto msg =std::make_shared<std_msgs::msg::String>();

            int org_sockfd;
            int client_sockfd;
            struct sockaddr_in addr{};

            socklen_t len = sizeof(struct sockaddr_in);
            struct sockaddr_in from_addr;

            char buf[1024];
            memset(buf, 0, sizeof(buf));

            if ((org_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                RCLCPP_ERROR(this->get_logger(), "TCP Socket Creation Error");
                return;
            }

            int enable = 1;
            if (setsockopt(org_sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
                RCLCPP_ERROR(this->get_logger(), "TCP Socket setsockopt Error");
                return;
            }

            memset(&addr, 0, sizeof(struct sockaddr_in));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = INADDR_ANY;

            if (bind(org_sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
                RCLCPP_ERROR(this->get_logger(), "TCP Socket Bind Error when binding port:%d", port);
                return;
            }

            if (listen(org_sockfd, SOMAXCONN) < 0) {
                RCLCPP_ERROR(this->get_logger(), "TCP Socket Listen Error");
            }

            uint64_t counter = 1;
            write(newconnection_eventfd, &counter, sizeof(counter));    // tells handleservice_ that socket is made
            RCLCPP_INFO(this->get_logger(), "TCP Socket Binded to Port:%d", port);

            if ((client_sockfd = accept(org_sockfd, (struct sockaddr *) &from_addr, &len)) < 0) {
                RCLCPP_ERROR(this->get_logger(), "TCP Socket Accept Error");
            }

            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &from_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            RCLCPP_INFO(this->get_logger(), "TCP Socket Attatched from IP:%s", client_ip);

            publisher->on_activate();

            bool BREAK_FLAG = false;
            while (true) {
                const unsigned int MAX_BUF_LENGTH = 4096;
                std::vector<char> buffer(MAX_BUF_LENGTH);
                std::string rcv;
                int bytesReceived = 0;
                do {
                    bytesReceived = recv(client_sockfd, &buffer[0], buffer.size(), 0);
                    if (bytesReceived == 0){
                        BREAK_FLAG = true;
                        break;
                    }if ( bytesReceived == -1 ){    // Error
                        BREAK_FLAG = true;
                        RCLCPP_ERROR(this->get_logger(), "TCP Socket Recv Error");
                        break;
                    } else {
                        rcv.append( buffer.cbegin(), buffer.cend() );
                    }
                } while ( bytesReceived == MAX_BUF_LENGTH );
                if (BREAK_FLAG){
                    break;
                }
                msg->data = rcv;
                publisher->publish(*msg);
                // RCLCPP_INFO(this->get_logger(), "Receiving TCP message:%s", rcv.c_str());
            }
            publisher->on_deactivate();
            close(client_sockfd);
            close(org_sockfd);

            RCLCPP_INFO(this->get_logger(), "TCP Socket detached");

            detach_thread(port);
            return;
        }
    }

    void TcpInterface::handleService_(const std::shared_ptr<tcp_interface::srv::TcpSocketICtrl::Request> &request,
                                      const std::shared_ptr<tcp_interface::srv::TcpSocketICtrl::Response> &response) {
        newconnection_eventfd = eventfd(0, EFD_CLOEXEC);
        if (newconnection_eventfd == -1){
            RCLCPP_ERROR(this->get_logger(), "EFD CREATION ERROR");
            response->ack = false;
            return;
        }

        // register port to tcp thread
        uint16_t opening_port = request->port;
        pthread_t parent_pthread_t = pthread_self();
        thread_map_mtx_.lock();
        if (tcp_port_threads.find(opening_port) != tcp_port_threads.end()) { // if thread already exists
            if (tcp_port_threads[opening_port]->joinable()) {
                response->ack = true;
                return;
            }
        }
        tcp_port_threads[opening_port] = std::make_unique<std::thread>(&TcpInterface::server_thread, this, opening_port,
                                                                       parent_pthread_t);   // bundle port to thread object
        thread_map_mtx_.unlock();
        // wait for 100ms before timeout.
        int epfd = epoll_create1(0);
        if (epfd == -1) {
            close(newconnection_eventfd);
            RCLCPP_ERROR(this->get_logger(), "EPOLL_CREATE ERROR");
            RCLCPP_ERROR(this->get_logger(), "error:%s", strerror(errno));
            response->ack = false;
            close(epfd);
            return;
        }

        struct epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = newconnection_eventfd;
        epoll_ctl(epfd, EPOLL_CTL_ADD, newconnection_eventfd, &ev);

        int timeout_ms = 10000;
        int nfds = epoll_wait(epfd, &ev, 1, timeout_ms);
        if (nfds == -1) {
            RCLCPP_ERROR(this->get_logger(), "EPOLL ERROR");
            RCLCPP_ERROR(this->get_logger(), "error:%s", strerror(errno));
            response->ack = false;
        } else if (nfds == 0) {
            RCLCPP_ERROR(this->get_logger(), "SIGWAIT TIMEOUT");
            RCLCPP_ERROR(this->get_logger(), "error:%s", strerror(errno));
            response->ack = false;
        } else {
            uint64_t counter;
            read(newconnection_eventfd, &counter, sizeof(counter));
            response->ack = true;
        }
        close(epfd);
    }

    void TcpInterface::detach_thread(int port) {
        std::lock_guard<std::recursive_mutex> lock(thread_map_mtx_);
        tcp_port_threads[port]->detach();
    }
}

RCLCPP_COMPONENTS_REGISTER_NODE(tcp_interface::TcpInterface)