#pragma once

#include "core/handlers/iprotocol_handler.hpp"
#include "definitions.hpp"

namespace Tins {
    class IP;
    class IPv6;
    class ICMP;
    class ICMPv6;
}

class IcmpHandler final : public IProtocolHandler {
public:
    IcmpHandler(
        asio::any_io_executor executor,
        IPPacketChannel &outgoing_channel,
        const std::string& tun_address_v4,
        const std::string& tun_address_v6
    );

    void handle_ipv4(std::unique_ptr<Tins::IP> packet) override;
    void handle_ipv6(std::unique_ptr<Tins::IPv6> packet) override;

private:
    template <typename IpPacket, typename IcmpPacket>
    void process_echo_request(std::unique_ptr<IpPacket> packet);

    template <typename IpPacket, typename IcmpPacket>
    asio::awaitable<void> send_echo_reply(std::unique_ptr<IpPacket> request);

    asio::any_io_executor executor_;
    IPPacketChannel& outgoing_channel_;
    asio::ip::address_v4 tun_address_v4_;
    asio::ip::address_v6 tun_address_v6_;
};