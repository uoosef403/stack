#pragma once
#include "clients/iproxy_client.hpp"
#include "core/handlers/tcp/pcb.hpp"

namespace stack::proxy {
    // Represents a single, end-to-end proxied TCP connection.
    class TcpFlow {
    public:
        // Takes ownership of a client backend and references to the PCB and outgoing channel.
        TcpFlow(std::unique_ptr<clients::IProxyClient> client,
                stack::tcp::Pcb& pcb,
                IPPacketChannel& outgoing_channel);

        // Starts the proxy connection and the data relay coroutines.
        asio::awaitable<void> run();

    private:
        // These will shuttle data back and forth.
        asio::awaitable<void> relay_to_proxy();
        asio::awaitable<void> relay_to_client();

        std::unique_ptr<clients::IProxyClient> client_;
        stack::tcp::Pcb& pcb_;
        IPPacketChannel& outgoing_channel_;
		TcpHandler::FlowErrorCallback on_error_;
        asio::ip::tcp::socket real_socket_; // The real socket to the proxy server
    };
}