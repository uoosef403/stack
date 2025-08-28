#include "client.hpp"
#include <fmt/core.h>

namespace stack::proxy::clients {
    Socks5Client::Socks5Client(asio::any_io_executor executor, asio::ip::tcp::endpoint endpoint)
        : executor_(std::move(executor)), endpoint_(std::move(endpoint)) {}

    asio::awaitable<asio::ip::tcp::socket> Socks5Client::connect(
        const tcp::four_tuple& destination)
    {
        // Create a new socket for this specific connection attempt.
        asio::ip::tcp::socket socket(executor_);

        // Connect to the SOCKS5 proxy server itself.
        co_await socket.async_connect(endpoint_, asio::use_awaitable);

        // --- SOCKS5 Handshake ---

        // 1. Client Hello: Send supported authentication methods.
        const uint8_t hello[] = {0x05, 1, 0x00};
        co_await asio::async_write(socket, asio::buffer(hello), asio::use_awaitable);

        // 2. Server Choice: Receive the authentication method chosen by the server.
        uint8_t choice[2];
        co_await asio::async_read(socket, asio::buffer(choice), asio::use_awaitable);
        if (choice[0] != 0x05 || choice[1] != 0x00) {
            throw std::runtime_error("SOCKS5 server chose an unsupported authentication method.");
        }

        // 3. Connection Request: Tell the proxy where we want to connect.
        std::vector<uint8_t> request;
        // SOCKS V5, Command CONNECT, Reserved (0x00), Address Type IPv4
        request.insert(request.end(), {0x05, 0x01, 0x00, 0x01});
        // Append the destination IP address bytes
        auto dest_ip_bytes = destination.dst_ip.to_v4().to_bytes(); // Use .dst_ip
        request.insert(request.end(), dest_ip_bytes.begin(), dest_ip_bytes.end());
        // Append the destination port in network byte order (big-endian)
        uint16_t dest_port_net = htons(destination.dst_port); // Use .dst_port
        request.push_back(dest_port_net >> 8);
        request.push_back(dest_port_net & 0xFF);
        co_await asio::async_write(socket, asio::buffer(request), asio::use_awaitable);

        // 4. Server Reply: Read the proxy's response to our request.
        uint8_t reply[4];
        co_await asio::async_read(socket, asio::buffer(reply), asio::use_awaitable);
        if (reply[1] != 0x00) { // Status byte 0x00 means "succeeded"
             throw std::runtime_error("SOCKS5 proxy failed to connect to the destination.");
        }

        fmt::println("SOCKS5: Connection successful through proxy to {}:{}",
            destination.dst_ip.to_string(), destination.dst_port); // Use .dst_ip and .dst_port

        // The handshake is complete. The socket is now transparently connected
        // to the destination. Return it to the caller.
        co_return socket;
    }
}