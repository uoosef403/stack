#pragma once
#include "iprotocol_handler.hpp"
#include "definitions.hpp"
#include "tcp/pcb.hpp"
#include <map>

class TcpHandler final : public IProtocolHandler {
public:
    TcpHandler(
        asio::any_io_executor executor,
        IPPacketChannel& outgoing_channel
    );

    void handle_ipv4(std::unique_ptr<Tins::IP> packet) override;
    void handle_ipv6(std::unique_ptr<Tins::IPv6> packet) override;

private:
    asio::any_io_executor executor_;
    IPPacketChannel& outgoing_channel_;

    // Map to store and look up all active connections.
    std::map<stack::tcp::four_tuple, stack::tcp::Pcb> connections_;
};