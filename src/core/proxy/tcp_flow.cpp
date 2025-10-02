#include "tcp_flow.hpp"
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <fmt/core.h>

namespace stack::proxy {
    TcpFlow::TcpFlow(std::unique_ptr<clients::IProxyClient> client, tcp::Pcb &pcb,
                     FlowErrorCallback on_error)
        : client_(std::move(client)), pcb_(pcb),
          on_error_(std::move(on_error)), // Initialize with the same executor
          real_socket_(pcb.executor_) {
    }

    asio::awaitable<void> TcpFlow::run() {
        try {
            fmt::println("TCP Flow: Starting for {}:{}",
                         pcb_.id.dst_ip().to_string(), pcb_.id.dst_port());

            // 1. Use the client to connect to the proxy.
            real_socket_ = co_await client_->connect(pcb_.id);

            // 2. If successful, spawn the two data relay coroutines.
            asio::co_spawn(pcb_.executor_, relay_to_proxy(), asio::detached);
            asio::co_spawn(pcb_.executor_, relay_to_client(), asio::detached);
        } catch (const std::exception &e) {
            fmt::println(stderr, "TCP Flow: Error for {}:{}: {}. Signaling handler.",
                     pcb_.id.dst_ip().to_string(), pcb_.id.dst_port(), e.what());

            // Call the tcp handler to report the problem
            on_error_(pcb_.id);
        }
        co_return;
    }

    asio::awaitable<void> TcpFlow::relay_to_proxy() {
        try {
            while (true) {
                // Read from the buffer fed by the TcpHandler
                auto [ec, packet_buffer_ptr] = co_await pcb_.ctp_buf.async_receive(asio::as_tuple);

                if (ec) {
                    // This is the signal that the client has sent a FIN and the
                    // TcpHandler has closed the channel.
                    if (ec == asio::experimental::channel_errc::channel_closed) {
                        fmt::println("TCP Flow: Client closed connection. Shutting down the send side of the proxy socket.");
                        
                        // Perform a graceful shutdown of the sending side.
                        // This sends a FIN packet on the real TCP connection.
                        boost::system::error_code shutdown_ec;
                        // Assign the return value to your variable to satisfy the compiler, stupid asio
                        shutdown_ec = real_socket_.shutdown(asio::ip::tcp::socket::shutdown_send, shutdown_ec);
                        if (shutdown_ec) {
                            fmt::println(stderr, "TCP Flow: Error shutting down socket: {}", shutdown_ec.message());
                        }
                    } else {
                        fmt::println(stderr, "TCP Flow: Error reading from client buffer: {}", ec.message());
                    }
                    
                    // For any error or channel closure, we are done sending data.
                    break;
                }

                // Write the received data to the real OS socket
                co_await asio::async_write(real_socket_, asio::buffer(*packet_buffer_ptr));
            }
        } catch (const std::exception& e) {
            fmt::println(stderr, "TCP Flow: Exception relaying to proxy: {}", e.what());
            on_error_(pcb_.id);
        }
    }

    asio::awaitable<void> TcpFlow::relay_to_client() {
        try {
            while (true) {
                auto buffer_ptr = std::make_unique<std::vector<uint8_t>>(2048);

                // Read data from the real socket directly into the heap buffer.
                auto [ec, bytes_transferred] = co_await real_socket_.async_read_some(
                    asio::buffer(*buffer_ptr), asio::as_tuple);

                if (ec) {
                    if (ec == asio::error::eof) {
                        // Remote end closed gracefully. Signal our handler by closing the channel.
                        pcb_.ptc_buf.close();
                    }
                    // For any error, we stop this relay.
                    break;
                }

                buffer_ptr->resize(bytes_transferred);

                // Move the unique_ptr into the channel, handing off ownership.
                co_await pcb_.ptc_buf.async_send({}, std::move(buffer_ptr));
            }
        } catch (const std::exception& e) {
            fmt::println(stderr, "TCP Flow: Exception relaying to client: {}", e.what());
            on_error_(pcb_.id);
        }
    }
} // namespace stack::proxy
