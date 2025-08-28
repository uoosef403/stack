#include "tcp_handler.hpp"
#include <fmt/core.h>
#include <random> // For generating ISN

// Anonymous namespace for internal helpers
namespace {
    // Helper to generate a random Initial Sequence Number.
    uint32_t generate_isn() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> dis;
        return dis(gen);
    }
} // namespace

namespace stack::handlers {
    // --- Constructor ---
    TcpHandler::TcpHandler(asio::any_io_executor executor,
                           IPPacketChannel &outgoing_channel)
        : executor_(std::move(executor)), outgoing_channel_(outgoing_channel) {
    }

    // --- Public Dispatcher Methods ---
    void TcpHandler::handle_ipv4(std::unique_ptr<Tins::IP> packet) {
        process_tcp_packet<Tins::IP>(std::move(packet));
    }

    void TcpHandler::handle_ipv6(std::unique_ptr<Tins::IPv6> packet) {
        process_tcp_packet<Tins::IPv6>(std::move(packet));
    }

    // --- Private Core Logic ---
    template<typename IP_TYPE>
    void TcpHandler::process_tcp_packet(std::unique_ptr<IP_TYPE> packet)
        requires(std::is_same_v<IP_TYPE, Tins::IP> ||
                 std::is_same_v<IP_TYPE, Tins::IPv6>) {
        const auto *tcp = packet->template find_pdu<Tins::TCP>();
        if (!tcp) {
            return; // Not a TCP packet
        }

        tcp::four_tuple tuple(*packet);
        const auto it = connections_.find(tuple);

        // --- Handle new connection attempts ---
        if (it == connections_.end()) {
            if (tcp->flags() == Tins::TCP::SYN) {
                handle_syn(*packet);
            }
            // Silently drop other packets for non-existent connections.
            return;
        }

        // --- Handle existing connections ---
        tcp::Pcb &pcb = it->second;

        // State: SYN_RECEIVED (completing the handshake)
        if (pcb.state == TcpState::SYN_RECEIVED && tcp->flags() == Tins::TCP::ACK) {
            if (tcp->ack_seq() == pcb.send_vars.next) {
                fmt::println("TCP: Connection ESTABLISHED for {}:{}",
                             tuple.src_ip.to_string(), tuple.src_port);
                pcb.state = TcpState::ESTABLISHED;
                pcb.send_vars.unacknowledged = tcp->ack_seq();
            }
            return; // Handshake ACK has no payload, so we can return.
        }

        // State: LAST_ACK (final ACK from client after we sent FIN)
        if (pcb.state == TcpState::LAST_ACK && tcp->flags() == Tins::TCP::ACK) {
            fmt::println("TCP: Connection closed for {}:{}",
                         tuple.src_ip.to_string(), tuple.src_port);
            connections_.erase(it); // Connection is fully closed.
            return;
        }

        // Ignore packets if not yet established (and not part of above states).
        if (pcb.state != TcpState::ESTABLISHED &&
            pcb.state != TcpState::CLOSE_WAIT) {
            return;
        }

        // --- Data Transfer (payload handling) ---
        const Tins::RawPDU *payload = tcp->template find_pdu<Tins::RawPDU>();
        if (payload && !payload->payload().empty()) {
            const auto &data = payload->payload();
            std::string data_str(data.begin(), data.end());

            fmt::println("TCP: Received {} bytes from {}:{}: \"{}\"", data.size(),
                         tuple.src_ip.to_string(), tuple.src_port, data_str);

            // Update acknowledgment number and send an ACK for the received data.
            pcb.recv_vars.next += static_cast<uint32_t>(data.size());
            asio::co_spawn(executor_, send_tcp_packet(pcb, Tins::TCP::ACK),
                           asio::detached);
        }

        // --- Connection Teardown (FIN handling) ---
        if (tcp->flags() & Tins::TCP::FIN) {
            fmt::println("TCP: Received FIN from {}:{}", tuple.src_ip.to_string(),
                         tuple.src_port);

            // Acknowledge their FIN.
            ++pcb.recv_vars.next;
            pcb.state = TcpState::CLOSE_WAIT;
            asio::co_spawn(executor_, send_tcp_packet(pcb, Tins::TCP::ACK),
                           asio::detached);

            // Immediately pretend our application is done and send our own FIN.
            pcb.state = TcpState::LAST_ACK;
            ++pcb.send_vars.next;
            asio::co_spawn(executor_,
                           send_tcp_packet(pcb, Tins::TCP::FIN | Tins::TCP::ACK),
                           asio::detached);
        }
    }

    template<typename IP_TYPE>
    void TcpHandler::handle_syn(const IP_TYPE &ip_packet)
        requires(std::is_same_v<IP_TYPE, Tins::IP> ||
                 std::is_same_v<IP_TYPE, Tins::IPv6>) {
        tcp::four_tuple tuple(ip_packet);
        const auto *tcp_syn = ip_packet.template find_pdu<Tins::TCP>();

        fmt::println("TCP: Received SYN from {}:{}", tuple.src_ip.to_string(),
                     tuple.src_port);

        // 1. Construct the Pcb directly inside the map.
        auto [it, success] = connections_.try_emplace(tuple, executor_, tuple);
        if (!success) {
            fmt::println(stderr, "Error: tcp connection already existed or some internal error occurred");
            return; // Connection already exists or is being set up.
        }

        // 2. Get a reference to the newly created Pcb inside the map.
        tcp::Pcb &pcb = it->second;

        // 3. Configure the new Pcb.
        pcb.state = TcpState::SYN_RECEIVED;
        pcb.recv_vars.next = tcp_syn->seq() + 1;
        const uint32_t our_isn = generate_isn();
        pcb.send_vars.unacknowledged = our_isn;
        pcb.send_vars.next = our_isn + 1;

        // 4. Send the SYN/ACK reply using the Pcb that's already in the map.
        asio::co_spawn(executor_,
                       send_tcp_packet(pcb, Tins::TCP::SYN | Tins::TCP::ACK),
                       asio::detached);
    }

    asio::awaitable<void>
    TcpHandler::send_tcp_packet(const tcp::Pcb &pcb, unsigned short flags,
                                const Tins::PDU *payload) const {
        try {
            Tins::TCP tcp(pcb.id.src_port, pcb.id.dst_port);
            tcp.flags(flags);

            // Set sequence number based on what flags are set.
            // For SYN or FIN, we use 'next-1', otherwise we use the last ack'd seq.
            if ((flags & Tins::TCP::SYN) || (flags & Tins::TCP::FIN)) {
                tcp.seq(pcb.send_vars.next - 1);
            } else {
                tcp.seq(pcb.send_vars.unacknowledged);
            }

            tcp.ack_seq(pcb.recv_vars.next);
            tcp.window(pcb.recv_vars.window);

            if (payload) {
                tcp.inner_pdu(payload->clone());
            }

            Tins::byte_array raw_response;
            if (pcb.id.is_ipv6()) {
                Tins::IPv6 ip6(pcb.id.dst_ip.to_string(),
                               pcb.id.src_ip.to_string());
                ip6.inner_pdu(tcp);
                raw_response = ip6.serialize();
            } else {
                Tins::IP ip4(pcb.id.dst_ip.to_string(), pcb.id.src_ip.to_string());
                ip4.inner_pdu(tcp);
                raw_response = ip4.serialize();
            }

            auto response_ptr = std::make_unique<std::vector<uint8_t> >(
                raw_response.begin(), raw_response.end());

            co_await outgoing_channel_.async_send({}, std::move(response_ptr));
        } catch (const std::exception &e) {
            fmt::println(stderr, "Error sending TCP packet: {}", e.what());
        }
    }

    void TcpHandler::terminate_connection(const tcp::four_tuple& id) {
        const auto pcb_it = connections_.find(id);
        if (pcb_it == connections_.end()) {
            return; // Already cleaned up
        }

        tcp::Pcb& pcb = pcb_it->second;
        fmt::println("TCP Handler: Terminating connection for {}:{}",
                     id.src_ip.to_string(), id.src_port);

        // Send an RST packet
        // Note: The sequence number for an RST is often just the last acknowledged number.
        asio::co_spawn(
            executor_,
            send_tcp_packet(pcb, Tins::TCP::RST | Tins::TCP::ACK),
            asio::detached
        );

        // Clean up the state
        flows_.erase(id);
        connections_.erase(id);
    }
} // namespace stack::handlers
