#include "tcp_handler.hpp"
#include <fmt/core.h>
#include <random> // For generating ISN

namespace stack::proxy { class TcpFlow; }

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
                           IPPacketChannel &outgoing_channel,
                           ProxyClientFactory factory)
        : executor_(std::move(executor)), outgoing_channel_(outgoing_channel),
          proxy_client_factory_(std::move(factory)) {
    }

    TcpHandler::~TcpHandler() = default;

    // --- Public Dispatcher Methods ---
    void TcpHandler::handle_ipv4(std::unique_ptr<Tins::IP> packet) {
        // A coroutine must be spawned to run.
        asio::co_spawn(executor_,
                       process_tcp_packet<Tins::IP>(std::move(packet)),
                       asio::detached);
    }

    void TcpHandler::handle_ipv6(std::unique_ptr<Tins::IPv6> packet) {
        // Also spawn the IPv6 version.
        asio::co_spawn(executor_,
                       process_tcp_packet<Tins::IPv6>(std::move(packet)),
                       asio::detached);
    }

    // --- Private Core Logic ---
    template<typename IP_TYPE>
    asio::awaitable<void> TcpHandler::process_tcp_packet(std::unique_ptr<IP_TYPE> packet)
        requires(std::is_same_v<IP_TYPE, Tins::IP> ||
                 std::is_same_v<IP_TYPE, Tins::IPv6>) {
        const auto *tcp = packet->template find_pdu<Tins::TCP>();
        if (!tcp) co_return;

        tcp::four_tuple tuple(*packet);
        const auto pcb_it = connections_.find(tuple);

        // --- Handle new connection attempts ---
        if (pcb_it == connections_.end()) {
            if (tcp->flags() == Tins::TCP::SYN) {
                handle_syn(*packet);
            }
            co_return;
        }

        // --- Handle existing connections ---
        tcp::Pcb &pcb = pcb_it->second;

        // Optional but recommended: Basic sequence number validation
        // Ignores packets that are not the next expected sequence, except for pure ACKs.
        bool has_payload = tcp->template find_pdu<Tins::RawPDU>() != nullptr &&
                           !tcp->template find_pdu<Tins::RawPDU>()->payload().empty();

        if ((has_payload || (tcp->flags() & Tins::TCP::FIN)) && tcp->seq() != pcb.recv_vars.next) {
            fmt::println("TCP: Ignoring out-of-order packet for {}:{}",
                         tuple.src_ip.to_string(), tuple.src_port);
            // In a real stack, you might send a duplicate ACK here.
            co_return;
        }

        // Use a state machine to handle the packet based on the connection's current state.
        switch (pcb.state) {
            case TcpState::SYN_RECEIVED: {
                if (tcp->flags() == Tins::TCP::ACK && tcp->ack_seq() == pcb.send_vars.next) {
                    fmt::println("TCP: Connection ESTABLISHED for {}:{}",
                                 tuple.src_ip.to_string(), tuple.src_port);
                    pcb.state = TcpState::ESTABLISHED;
                    pcb.send_vars.unacknowledged = tcp->ack_seq();

                    // The connection is established, so we create and launch the high-level flow.
                    if (!flows_.contains(tuple)) {
                        auto on_error_callback = [this](const tcp::four_tuple &id) {
                            this->terminate_connection(id);
                        };

                        auto client = proxy_client_factory_();
                        auto flow = std::make_unique<proxy::TcpFlow>(
                            std::move(client), pcb, outgoing_channel_, on_error_callback
                        );

                        // Launch the flow's main run loop, but don't wait for it.
                        asio::co_spawn(executor_, flow->run(), asio::detached);
                        flows_[tuple] = std::move(flow); // Store the active flow.
                    }
                }
                break;
            }

            case TcpState::ESTABLISHED: {
                // Handle incoming data from the client application.
                if (const Tins::RawPDU *payload = tcp->template find_pdu<Tins::RawPDU>();
                    payload && !payload->payload().empty()) {
                    const auto &data = payload->payload();

                    // Pass(copy the packet) the data UP to the TcpFlow via the PCB's buffer channel.
                    co_await pcb.ctp_buf.async_send(
                        {},
                        std::make_unique<std::vector<uint8_t> >(data.begin(), data.end())
                    );

                    // Acknowledge the data we just received.
                    pcb.recv_vars.next += static_cast<uint32_t>(data.size());
                    asio::co_spawn(executor_, send_tcp_packet(pcb, Tins::TCP::ACK), asio::detached);
                }

                // Handle the client wanting to close the connection.
                if (tcp->flags() & Tins::TCP::FIN) {
                    fmt::println("TCP: Received FIN from {}:{}", tuple.src_ip.to_string(), tuple.src_port);

                    // Acknowledge their FIN.
                    ++pcb.recv_vars.next;
                    asio::co_spawn(executor_, send_tcp_packet(pcb, Tins::TCP::ACK), asio::detached);

                    // Transition state and signal our side (the TcpFlow) to shut down.
                    pcb.state = TcpState::CLOSE_WAIT;
                    pcb.ctp_buf.close(); // This signals the end of data to the TcpFlow.
                }
                break;
            }

            case TcpState::LAST_ACK: {
                // This is the final ACK from the client, acknowledging our FIN.
                if (tcp->flags() == Tins::TCP::ACK && tcp->ack_seq() == pcb.send_vars.next) {
                    fmt::println("TCP: Connection fully closed for {}:{}",
                                 tuple.src_ip.to_string(), tuple.src_port);
                    // The connection is done. Clean up all state.
                    connections_.erase(pcb_it);
                    flows_.erase(tuple);
                }
                break;
            }

            // Other states like CLOSE_WAIT, FIN_WAIT_1, etc., would be handled here.
            // For our simple proxy, this covers the main paths.
            default:
                break;
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
    TcpHandler::send_tcp_packet(const tcp::Pcb &pcb, const unsigned short flags,
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

    void TcpHandler::terminate_connection(const tcp::four_tuple &id) {
        const auto pcb_it = connections_.find(id);
        if (pcb_it == connections_.end()) {
            return; // Already cleaned up
        }

        tcp::Pcb &pcb = pcb_it->second;
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
