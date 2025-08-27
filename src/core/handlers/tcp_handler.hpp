#pragma once
#include "iprotocol_handler.hpp"
#include "definitions.hpp"
#include "tcp/pcb.hpp"
#include <map>

class TcpHandler final : public IProtocolHandler {
public:
    TcpHandler(
        asio::any_io_executor executor,
        IPPacketChannel &outgoing_channel
    );

    void handle_ipv4(std::unique_ptr<Tins::IP> packet) override;

    void handle_ipv6(std::unique_ptr<Tins::IPv6> packet) override;

private:
    asio::any_io_executor executor_;
    IPPacketChannel &outgoing_channel_;


    template<class IP_TYPE>
    void process_tcp_packet(std::unique_ptr<IP_TYPE> packet)
        requires (std::is_same_v<IP_TYPE, Tins::IP> || std::is_same_v<IP_TYPE, Tins::IPv6>);

    template<typename IP_TYPE>
    void handle_syn(const IP_TYPE &ip_packet)
        requires (std::is_same_v<IP_TYPE, Tins::IP> || std::is_same_v<IP_TYPE, Tins::IPv6>);

    asio::awaitable<void> send_tcp_packet(const stack::tcp::Pcb &pcb, uint32_t flags,
                                          const Tins::PDU *payload = nullptr) const;

    // Map to store and look up all active connections.
    std::map<stack::tcp::four_tuple, stack::tcp::Pcb> connections_;
};
