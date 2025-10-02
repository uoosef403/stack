#include "client.hpp"
#include <fmt/core.h>

namespace stack::proxy::clients {

    Socks5Client::Socks5Client(asio::any_io_executor executor, asio::ip::tcp::endpoint endpoint)
        : executor_(std::move(executor)), endpoint_(std::move(endpoint)) {}

    asio::awaitable<asio::ip::tcp::socket> Socks5Client::connect(
        const network::ConnectionId& destination)
    {
        using asio::use_awaitable;

        asio::ip::tcp::socket socket(executor_);

        // 1. Connect to the SOCKS5 proxy
        co_await socket.async_connect(endpoint_, use_awaitable);

        // 2. Send client hello (no authentication)
        const uint8_t hello[] = {0x05, 0x01, 0x00};
        co_await asio::async_write(socket, asio::buffer(hello), use_awaitable);

        // 3. Read server choice
        uint8_t choice[2];
        co_await asio::async_read(socket, asio::buffer(choice), use_awaitable);
        if (choice[0] != 0x05 || choice[1] != 0x00)
            throw std::runtime_error("SOCKS5: unsupported authentication method");

        // 4. Send connection request
        std::vector<uint8_t> request;
        request.reserve(10);

        request.push_back(0x05); // VER
        request.push_back(0x01); // CMD = CONNECT
        request.push_back(0x00); // RSV

        if (destination.dst_ip().is_v4()) {
            request.push_back(0x01); // ATYP = IPv4
            auto bytes = destination.dst_ip().to_v4().to_bytes();
            request.insert(request.end(), bytes.begin(), bytes.end());
        } else if (destination.dst_ip().is_v6()) {
            request.push_back(0x04); // ATYP = IPv6
            auto bytes = destination.dst_ip().to_v6().to_bytes();
            request.insert(request.end(), bytes.begin(), bytes.end());
        } else {
            throw std::runtime_error("SOCKS5: invalid destination address");
        }

        uint16_t port_be = destination.dst_port();

        request.push_back(static_cast<uint8_t>(port_be >> 8));
        request.push_back(static_cast<uint8_t>(port_be & 0xFF));

        co_await asio::async_write(socket, asio::buffer(request), use_awaitable);

        // 5. Read server reply
        uint8_t header[4];
        co_await asio::async_read(socket, asio::buffer(header), use_awaitable);

        if (header[0] != 0x05)
            throw std::runtime_error("SOCKS5: invalid version in reply");
        if (header[1] != 0x00)
            throw std::runtime_error(fmt::format("SOCKS5: connection failed (REP={:02x})", header[1]));

        uint8_t atyp = header[3];

        // Read BND.ADDR depending on ATYP
        if (atyp == 0x01) { // IPv4
            uint8_t addr[4];
            co_await asio::async_read(socket, asio::buffer(addr), use_awaitable);
        } else if (atyp == 0x04) { // IPv6
            uint8_t addr[16];
            co_await asio::async_read(socket, asio::buffer(addr), use_awaitable);
        } else if (atyp == 0x03) { // DOMAIN
            uint8_t len;
            co_await asio::async_read(socket, asio::buffer(&len, 1), use_awaitable);
            std::vector<uint8_t> domain(len);
            co_await asio::async_read(socket, asio::buffer(domain), use_awaitable);
        } else {
            throw std::runtime_error("SOCKS5: unknown ATYP in reply");
        }

        // Read BND.PORT
        uint8_t port_bytes[2];
        co_await asio::async_read(socket, asio::buffer(port_bytes), use_awaitable);

        fmt::println("SOCKS5: connection established to {}:{}",
            destination.dst_ip().to_string(), destination.dst_port());

        // 6. Return ready-to-use socket
        co_return std::move(socket);
    }

} // namespace stack::proxy::clients
