#include "icmp_handler.hpp"
#include <fmt/core.h>
#include <tins/tins.h>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

namespace stack::handlers {

IcmpHandler::IcmpHandler(
    asio::any_io_executor executor,
    IPPacketChannel &outgoing_channel,
    const std::string& tun_address_v4,
    const std::string& tun_address_v6
) : executor_(std::move(executor)),
      outgoing_channel_(outgoing_channel) {
    tun_address_v4_ = asio::ip::make_address_v4(tun_address_v4);
    tun_address_v6_ = asio::ip::make_address_v6(tun_address_v6);
}

void IcmpHandler::handle_ipv4(std::unique_ptr<Tins::IP> packet) {
    process_echo_request<Tins::IP, Tins::ICMP>(std::move(packet));
}

void IcmpHandler::handle_ipv6(std::unique_ptr<Tins::IPv6> packet) {
    process_echo_request<Tins::IPv6, Tins::ICMPv6>(std::move(packet));
}

template<typename IpPacket, typename IcmpPacket>
void IcmpHandler::process_echo_request(std::unique_ptr<IpPacket> packet) {
    if (const auto *icmp = packet->template find_pdu<IcmpPacket>()) {
        if (icmp->type() == IcmpPacket::ECHO_REQUEST) {
            fmt::println("ICMP: Received ping from {}", packet->src_addr().to_string());

            // Spawn coroutine directly, safely moving ownership of packet
            asio::co_spawn(
                executor_,
                send_echo_reply<IpPacket, IcmpPacket>(std::move(packet)),
                asio::detached
            );
        }
    }
}

template<typename IpPacket, typename IcmpPacket>
asio::awaitable<void> IcmpHandler::send_echo_reply(std::unique_ptr<IpPacket> request) {
    try {
        const auto *icmp_request = request->template find_pdu<IcmpPacket>();
        if (!icmp_request) {
            co_return;
        }

        IcmpPacket icmp_reply;
        icmp_reply.type(IcmpPacket::ECHO_REPLY);
        if constexpr (std::is_same_v<IcmpPacket, Tins::ICMP>) {
            // IPv4 ICMP echo
            icmp_reply.id(icmp_request->id());
        } else if constexpr (std::is_same_v<IcmpPacket, Tins::ICMPv6>) {
            // IPv6 ICMPv6 echo
            icmp_reply.identifier(icmp_request->identifier());
        }

        icmp_reply.sequence(icmp_request->sequence());

        if (const Tins::PDU *payload = icmp_request->inner_pdu()) {
            icmp_reply.inner_pdu(payload->clone());
        }

        IpPacket ip_reply(request->src_addr(), request->dst_addr());
        ip_reply.inner_pdu(icmp_reply);

        auto raw_response = ip_reply.serialize();

        fmt::println("ICMP: Sending ping reply to {}", ip_reply.dst_addr().to_string());

        // Wrap into unique_ptr<vector<char>> for the channel
        auto response_ptr = std::make_unique<std::vector<uint8_t> >(raw_response.begin(), raw_response.end());
        co_await outgoing_channel_.async_send({}, std::move(response_ptr));
    } catch (const std::exception &e) {
        fmt::println(stderr, "Error sending ICMP reply: {}", e.what());
    }
}

}