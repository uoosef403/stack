#include "tcp_handler.hpp"
#include "core/logging/logger.hpp"
#include <random> // For generating ISN

namespace stack::proxy {
class TcpFlow;
}

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
    : executor_(std::move(executor)),
      device_outgoing_channel_(outgoing_channel),
      proxy_client_factory_(std::move(factory)) {}

TcpHandler::~TcpHandler() = default;

// --- Public Dispatcher Methods ---
void TcpHandler::handle_ipv4(std::unique_ptr<Tins::IP> packet) {
  // A coroutine must be spawned to run.
  asio::co_spawn(executor_, process_tcp_packet<Tins::IP>(std::move(packet)),
                 asio::detached);
}

void TcpHandler::handle_ipv6(std::unique_ptr<Tins::IPv6> packet) {
  // Also spawn the IPv6 version.
  asio::co_spawn(executor_, process_tcp_packet<Tins::IPv6>(std::move(packet)),
                 asio::detached);
}

// --- Private Core Logic ---
template <typename IP_TYPE>
asio::awaitable<void>
TcpHandler::process_tcp_packet(std::unique_ptr<IP_TYPE> packet)
  requires(std::is_same_v<IP_TYPE, Tins::IP> ||
           std::is_same_v<IP_TYPE, Tins::IPv6>)
{
  const auto *tcp = packet->template find_pdu<Tins::TCP>();
  if (!tcp)
    co_return;

  network::ConnectionId connection_id(*packet);
  const auto pcb_it = connections_.find(connection_id);

  // --- Handle new connection attempts ---
  if (pcb_it == connections_.end()) {
    if (tcp->flags() == Tins::TCP::SYN) {
      co_await handle_syn(*packet);
    }
    co_return;
  }

  // --- Handle existing connections ---
  tcp::Pcb &pcb = pcb_it->second;

  // Basic sequence number validation
  // Ignores packets that are not the next expected sequence, except for pure
  // ACKs.
  bool has_payload = tcp->template find_pdu<Tins::RawPDU>() != nullptr &&
                     !tcp->template find_pdu<Tins::RawPDU>()->payload().empty();

  if ((has_payload || (tcp->flags() & Tins::TCP::FIN)) &&
      tcp->seq() != pcb.recv_vars.next) {
    LOG_DEBUG("TCP: Ignoring out-of-order packet for {}:{}",
              connection_id.src_ip().to_string(), connection_id.src_port());
    // In a real stack, you might send a duplicate ACK here.
    co_return;
  }

  // Use a state machine to handle the packet based on the connection's current
  // state.
  switch (pcb.state) {
  case TcpState::SYN_RECEIVED: {
    if (tcp->flags() == Tins::TCP::ACK &&
        tcp->ack_seq() == pcb.send_vars.next) {
      LOG_INFO("TCP: Connection ESTABLISHED for {}:{}",
               connection_id.src_ip().to_string(), connection_id.src_port());
      pcb.state = TcpState::ESTABLISHED;
      pcb.send_vars.unacknowledged = tcp->ack_seq();

      // The connection is established, so we create and launch the high-level
      // flow.
      if (!flows_.contains(connection_id)) {
        auto on_error_callback = [this](const network::ConnectionId id) {
          asio::co_spawn(this->executor_, this->terminate_connection(id),
                         asio::detached);
        };

        auto client = proxy_client_factory_();
        auto flow = std::make_unique<proxy::TcpFlow>(std::move(client), pcb,
                                                     on_error_callback);

        // Launch the flow's main run loop, but don't wait for it.
        asio::co_spawn(executor_, flow->run(), asio::detached);
        flows_[connection_id] = std::move(flow); // Store the active flow.
        asio::co_spawn(executor_, send_flow_packets(connection_id),
                       asio::detached);
      }
    }
    break;
  }

  case TcpState::ESTABLISHED: {
    // Handle incoming data from the client application.
    if (const Tins::RawPDU *payload = tcp->template find_pdu<Tins::RawPDU>();
        payload && !payload->payload().empty()) {
      const auto &data = payload->payload();

      // Pass(copy the packet) the data UP to the TcpFlow via the PCB's buffer
      // channel.
      co_await pcb.ctp_buf.async_send(
          {}, std::make_unique<std::vector<uint8_t>>(data.begin(), data.end()));

      // Acknowledge the data we just received.
      pcb.recv_vars.next += static_cast<uint32_t>(data.size());
      co_await send_tcp_packet(pcb, Tins::TCP::ACK);
    }

    // Handle the client wanting to close the connection.
    if (tcp->flags() & Tins::TCP::FIN) {
      LOG_INFO("TCP: Received FIN from {}:{}",
               connection_id.src_ip().to_string(), connection_id.src_port());

      // Acknowledge their FIN.
      ++pcb.recv_vars.next;
      co_await send_tcp_packet(pcb, Tins::TCP::ACK);

      // Transition state and signal our side (the TcpFlow) to shut down.
      pcb.state = TcpState::CLOSE_WAIT;
      pcb.ctp_buf.close(); // This signals the end of data to the TcpFlow.
    }
    break;
  }

  case TcpState::LAST_ACK: {
    // This is the final ACK from the client, acknowledging our FIN.
    if (tcp->flags() == Tins::TCP::ACK &&
        tcp->ack_seq() == pcb.send_vars.next) {
      LOG_INFO("TCP: Connection fully closed for {}:{}",
               connection_id.src_ip().to_string(), connection_id.src_port());
      // The connection is done. Clean up all state.
      connections_.erase(pcb_it);
      flows_.erase(connection_id);
    }
    break;
  }

  // Other states like CLOSE_WAIT, FIN_WAIT_1, etc., would be handled here.
  // For our simple proxy, this covers the main paths.
  default:
    break;
  }
}

template <typename IP_TYPE>
asio::awaitable<void> TcpHandler::handle_syn(const IP_TYPE &ip_packet)
  requires(std::is_same_v<IP_TYPE, Tins::IP> ||
           std::is_same_v<IP_TYPE, Tins::IPv6>)
{
  network::ConnectionId connection_id(ip_packet);
  const auto *tcp_syn = ip_packet.template find_pdu<Tins::TCP>();

  LOG_INFO("TCP: Received SYN from {}:{}", connection_id.src_ip().to_string(),
           connection_id.src_port());

  // 1. Construct the Pcb directly inside the map.
  auto [it, success] =
      connections_.try_emplace(connection_id, executor_, connection_id);
  if (!success) {
    LOG_ERROR("TCP connection already existed or some internal error occurred");
    co_return; // Connection already exists or is being set up.
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
  co_await send_tcp_packet(pcb, Tins::TCP::SYN | Tins::TCP::ACK);
}

asio::awaitable<void>
TcpHandler::send_tcp_packet(const tcp::Pcb &pcb, const unsigned short flags,
                            const Tins::PDU *payload) const {
  try {
    Tins::TCP tcp(pcb.id.src_port(), pcb.id.dst_port());
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

    // swapping the source ip and dest ip in response packet to trick the kernel
    // in that the original remote host is responding.
    Tins::byte_array raw_response;
    if (pcb.id.is_ipv6()) {
      Tins::IPv6 ip6(pcb.id.src_ip().to_string(), pcb.id.dst_ip().to_string());
      ip6.inner_pdu(tcp);
      raw_response = ip6.serialize();
    } else {
      Tins::IP ip4(pcb.id.src_ip().to_string(), pcb.id.dst_ip().to_string());
      ip4.inner_pdu(tcp);
      raw_response = ip4.serialize();
    }

    auto response_ptr = std::make_unique<std::vector<uint8_t>>(
        raw_response.begin(), raw_response.end());

    co_await device_outgoing_channel_.async_send({}, std::move(response_ptr));
  } catch (const std::exception &e) {
    LOG_ERROR("Error sending TCP packet: {}", e.what());
  }
}

asio::awaitable<void> TcpHandler::send_flow_packets(network::ConnectionId id) {
  try {
    while (true) {
      auto pcb_it = connections_.find(id);
      if (pcb_it == connections_.end())
        co_return; // Connection is gone

      // 1. Read raw data from the channel fed by TcpFlow
      auto [ec, data] =
          co_await pcb_it->second.ptc_buf.async_receive(asio::as_tuple);

      if (ec) {
        // Channel was closed by TcpFlow, meaning the remote end sent a FIN.
        // We now send our FIN to the client.
        if (pcb_it->second.state == TcpState::ESTABLISHED) {
          pcb_it->second.state = TcpState::FIN_WAIT_1;
          co_await send_tcp_packet(pcb_it->second,
                                   Tins::TCP::FIN | Tins::TCP::ACK);
          pcb_it->second.send_vars.unacknowledged++;
        }
        break; // Exit the loop
      }

      // 2. Wrap the data in a TCP packet and send it
      Tins::RawPDU raw_pdu(*data);
      co_await send_tcp_packet(pcb_it->second, Tins::TCP::PSH | Tins::TCP::ACK,
                               &raw_pdu);

      // 3. Update the sequence number for the data we just sent
      pcb_it->second.send_vars.unacknowledged += data->size();
    }
  } catch (const std::exception &e) {
    LOG_ERROR("TCP Handler: Packetizer for {} failed: {}",
              id.src_ip().to_string(), e.what());
  }
}

asio::awaitable<void>
TcpHandler::terminate_connection(const network::ConnectionId &id) {
  const auto pcb_it = connections_.find(id);
  if (pcb_it == connections_.end()) {
    co_return; // Already cleaned up
  }

  tcp::Pcb &pcb = pcb_it->second;
  LOG_INFO("TCP Handler: Terminating connection for {}:{}",
           id.src_ip().to_string(), id.src_port());

  // Send an RST packet
  // Note: The sequence number for an RST is often just the last acknowledged
  // number.
  co_await send_tcp_packet(pcb, Tins::TCP::RST | Tins::TCP::ACK);

  // Clean up the state
  flows_.erase(id);
  connections_.erase(id);
}
} // namespace stack::handlers
