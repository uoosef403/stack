#pragma once

#include "definitions.hpp"
#include "iprotocol_handler.hpp"
#include "core/proxy/tcp_flow.hpp"
#include "tcp/pcb.hpp"

namespace stack::handlers {
    // A factory function that creates proxy client instances.
    using ProxyClientFactory = std::function<std::unique_ptr<proxy::clients::IProxyClient>()>;
    // A function that takes a connection ID and signals an error.
    using FlowErrorCallback = std::function<void(const network::ConnectionId&)>;

    class TcpHandler final : public IProtocolHandler {
    public:
        TcpHandler(
            asio::any_io_executor executor,
            IPPacketChannel &device_outgoing_channel_,
            ProxyClientFactory factory
        );

        ~TcpHandler() override;

        void handle_ipv4(std::unique_ptr<Tins::IP> packet) override;

        void handle_ipv6(std::unique_ptr<Tins::IPv6> packet) override;
    private:
        asio::any_io_executor executor_;
        IPPacketChannel &device_outgoing_channel_;

        ProxyClientFactory proxy_client_factory_;

        template<class IP_TYPE>
        asio::awaitable<void> process_tcp_packet(std::unique_ptr<IP_TYPE> packet)
            requires (std::is_same_v<IP_TYPE, Tins::IP> || std::is_same_v<IP_TYPE, Tins::IPv6>);

        template<typename IP_TYPE>
        asio::awaitable<void> handle_syn(const IP_TYPE &ip_packet)
            requires (std::is_same_v<IP_TYPE, Tins::IP> || std::is_same_v<IP_TYPE, Tins::IPv6>);

        asio::awaitable<void> send_tcp_packet(const tcp::Pcb &pcb, unsigned short flags,
                                              const Tins::PDU *payload = nullptr) const;

        asio::awaitable<void> send_flow_packets(network::ConnectionId id);

        asio::awaitable<void> terminate_connection(const network::ConnectionId& id);

        // Map to store and look up all active connections.
        std::map<network::ConnectionId, tcp::Pcb> connections_;
        std::map<network::ConnectionId, std::unique_ptr<proxy::TcpFlow>> flows_;
    };
}
