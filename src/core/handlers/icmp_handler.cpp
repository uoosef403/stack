#include "icmp_handler.hpp"

#include <fmt/core.h>
#include <tins/tins.h>

IcmpHandler::IcmpHandler(asio::any_io_executor executor,
                         IPPacketChannel &outgoing_channel,
                         const std::string& tun_address_v4,
                         const std::string& tun_address_v6)
    : executor_(std::move(executor)), outgoing_channel_(outgoing_channel) {
    tun_address_v4_ = asio::ip::make_address_v4(tun_address_v4);
    tun_address_v6_ = asio::ip::make_address_v6(tun_address_v6);
}

void IcmpHandler::handle_ipv4(Tins::IP &packet) {
    // We dont actually proxy icmp echo packets and we dont
    // care about them so we simply generate a fake response
    // for testing purposes

    // Use libtins to find the ICMP layer within the IP packet.
    if (const auto *icmp = packet.find_pdu<Tins::ICMP>()) {
        // Check if it's a ping (echo) request.
        if (icmp->type() == Tins::ICMP::ECHO_REQUEST) {
            fmt::println("ICMP: Received ping from {}",
                         packet.src_addr().to_string());

            // Launch a new coroutine to handle sending the reply.
            // This prevents blocking the main dispatcher loop if sending takes time.
            asio::co_spawn(executor_, send_echo_reply(packet), asio::detached);
        }
    }
}

void IcmpHandler::handle_ipv6(Tins::IPv6 &packet) {
    // We are not implementing IPv6 ping yet, so this is empty.
    (void) packet; // Suppress unused parameter warning.
}

asio::awaitable<void> IcmpHandler::send_echo_reply(const Tins::IP &request) const {
    try {
        const auto *icmp_request = request.find_pdu<Tins::ICMP>();
        if (!icmp_request) {
            co_return;
        }

        // 1. Manually create the ICMP echo reply PDU.
        Tins::ICMP icmp_reply;
        icmp_reply.type(Tins::ICMP::ECHO_REPLY);
        icmp_reply.id(icmp_request->id());
        icmp_reply.sequence(icmp_request->sequence());

        // 2. Correctly handle the payload.
        // The payload of an ICMP echo request is its "inner PDU". We find it
        // and attach a copy to our reply.
        if (const Tins::PDU *payload = icmp_request->inner_pdu()) {
            icmp_reply.inner_pdu(payload->clone());
        }

        // 3. Manually create the IP reply packet.
        Tins::IP ip_reply(
            request.src_addr(), // The new destination is the original source
            request.dst_addr() // The new source is the original destination
        );
        ip_reply.inner_pdu(icmp_reply);

        // 4. Serialize the complete packet.
        Tins::PDU::serialization_type raw_response = ip_reply.serialize();

        fmt::println("ICMP: Sending ping reply to {}", ip_reply.dst_addr().to_string());

        // 5. Send the buffer to the outgoing channel.
        co_await outgoing_channel_.async_send(
            boost::system::error_code{},
            std::vector(raw_response.begin(), raw_response.end()),
            asio::use_awaitable
        );
    } catch (const std::exception &e) {
        fmt::println(stderr, "Error sending ICMP reply: {}", e.what());
    }
}
