#pragma once

#include "core/proxy/clients/iproxy_client.hpp"

namespace stack::proxy::clients {
    class Socks5Client final : public IProxyClient {
    public:
        // The constructor takes the executor needed to create sockets and
        // the endpoint (IP and port) of the SOCKS5 proxy server.
        Socks5Client(asio::any_io_executor executor, asio::ip::tcp::endpoint endpoint);

        // The connect method overrides the pure virtual function in the interface.
        // It performs the full SOCKS5 handshake and returns a connected socket.
        asio::awaitable<asio::ip::tcp::socket> connect(
            const network::ConnectionId& destination
        ) override;

    private:
        asio::any_io_executor executor_;
        asio::ip::tcp::endpoint endpoint_;
    };
}