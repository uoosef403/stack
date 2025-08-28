#pragma once

#include "definitions.hpp"
#include "iprotocol_handler.hpp"
#include "core/proxy/tcp_flow.hpp"
#include "tcp/pcb.hpp"

namespace stack::handlers {
    class TcpHandler final : public IProtocolHandler {
    public:
        TcpHandler(
            asio::any_io_executor executor,
            IPPacketChannel &outgoing_channel
        );

        void handle_ipv4(std::unique_ptr<Tins::IP> packet) override;

        void handle_ipv6(std::unique_ptr<Tins::IPv6> packet) override;

        // A function that takes a connection ID and signals an error.
        using FlowErrorCallback = std::function<void(const tcp::four_tuple&)>;

    private:
        asio::any_io_executor executor_;
        IPPacketChannel &outgoing_channel_;


        template<class IP_TYPE>
        void process_tcp_packet(std::unique_ptr<IP_TYPE> packet)
            requires (std::is_same_v<IP_TYPE, Tins::IP> || std::is_same_v<IP_TYPE, Tins::IPv6>);

        template<typename IP_TYPE>
        void handle_syn(const IP_TYPE &ip_packet)
            requires (std::is_same_v<IP_TYPE, Tins::IP> || std::is_same_v<IP_TYPE, Tins::IPv6>);

        asio::awaitable<void> send_tcp_packet(const tcp::Pcb &pcb, unsigned short flags,
                                              const Tins::PDU *payload = nullptr) const;

        void terminate_connection(const tcp::four_tuple& id);

        // Map to store and look up all active connections.
        std::map<tcp::four_tuple, tcp::Pcb> connections_;
        std::map<tcp::four_tuple, std::unique_ptr<proxy::TcpFlow>> flows_;
    };
}
