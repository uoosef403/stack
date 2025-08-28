#include "tcp_flow.hpp"
#include <fmt/core.h>

namespace stack::proxy {
    TcpFlow::TcpFlow(std::unique_ptr<clients::IProxyClient> client, tcp::Pcb &pcb,
                     IPPacketChannel &outgoing_channel,
                     handlers::TcpHandler::FlowErrorCallback on_error)
        : client_(std::move(client)), pcb_(pcb),
          outgoing_channel_(outgoing_channel),
          on_error_(std::move(on_error)), // Initialize with the same executor
          real_socket_(pcb.executor_) {
    }

    asio::awaitable<void> TcpFlow::run() {
        try {
            fmt::println("TCP Flow: Starting for {}:{}",
                         pcb_.id.dst_ip.to_string(), pcb_.id.dst_port);

            // 1. Use the client to connect to the proxy.
            real_socket_ = co_await client_->connect(pcb_.id);

            // 2. If successful, spawn the two data relay coroutines.
            asio::co_spawn(pcb_.executor_, relay_to_proxy(), asio::detached);
            asio::co_spawn(pcb_.executor_, relay_to_client(), asio::detached);
        } catch (const std::exception &e) {
            fmt::println(stderr, "TCP Flow: Error for {}:{}: {}. Signaling handler.",
                     pcb_.id.dst_ip.to_string(), pcb_.id.dst_port, e.what());

            // Call the tcp handler to report the problem
            on_error_(pcb_.id);
        }
        co_return;
    }

    asio::awaitable<void> TcpFlow::relay_to_proxy() {
        // TODO: Your next task will be to implement this.
        // It should loop, reading from `pcb_.client_to_proxy_buffer_`
        // and writing to `real_socket_`.
        co_return;
    }

    asio::awaitable<void> TcpFlow::relay_to_client() {
        // TODO: Your next task will be to implement this.
        // It should loop, reading from `real_socket_` and
        // creating TCP packets to send down the `outgoing_channel_`.
        co_return;
    }
} // namespace stack::proxy
