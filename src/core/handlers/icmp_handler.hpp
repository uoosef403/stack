#pragma once

#include "core/handlers/iprotocol_handler.hpp"
#include "definitions.hpp"
#include "tins/ip.h"

class IcmpHandler final : public IProtocolHandler {
public:
    IcmpHandler(asio::any_io_executor executor, IPPacketChannel &outgoing_channel,
                asio::ip::address_v4 tun_address);

    void handle_ipv4(Tins::IP &packet) override;

    void handle_ipv6(Tins::IPv6 &packet) override;

private:
    asio::awaitable<void> send_echo_reply(const Tins::IP &request) const;

    asio::any_io_executor executor_;
    IPPacketChannel &outgoing_channel_;
    asio::ip::address_v4 tun_address_;
};
