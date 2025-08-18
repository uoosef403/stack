#include "dispatcher.hpp"

#include <fmt/core.h>
#include <tins/tins.h>

Dispatcher::Dispatcher(const asio::any_io_executor &executor,
                       IPPacketChannel &incoming_channel,
                       IPPacketChannel &outgoing_channel)
    : executor_(executor), incoming_channel_(incoming_channel),
      outgoing_channel_(outgoing_channel) {
  fmt::print("Dispatcher started\n");
}

Dispatcher::~Dispatcher() { fmt::print("Dispatcher stopped\n"); }

asio::awaitable<void> Dispatcher::run() {
  try {
    while (true) {
      auto [ec, packet_data] = co_await incoming_channel_.async_receive(
          asio::as_tuple(asio::use_awaitable));

      if (ec || packet_data.empty())
        continue;

      uint8_t ip_version = (packet_data[0] >> 4);

      if (ip_version == 4) {
        try {
          Tins::IP ip_packet(packet_data.data(), static_cast<uint32_t>(packet_data.size()));
          if (auto it = handlers_.find(ip_packet.protocol());
              it != handlers_.end()) {
            it->second->handle_ipv4(ip_packet);
          }
        } catch (const Tins::malformed_packet &ex) {
          fmt::print(stderr, "   ERROR: libtins rejected IPv4 packet: {}\n",
                     ex.what());
        }
      } else if (ip_version == 6) {
        try {
          Tins::IPv6 ipv6_packet(packet_data.data(), static_cast<uint32_t>(packet_data.size()));
          if (auto it = handlers_.find(ipv6_packet.next_header());
              it != handlers_.end()) {
            it->second->handle_ipv6(ipv6_packet);
          }
        } catch (const Tins::malformed_packet &ex) {
          fmt::print(stderr, "   ERROR: libtins rejected IPv6 packet: {}\n",
                     ex.what());
        }
      } else {
        fmt::print("-> INFO: Dropping packet with unknown IP version: {}\n",
                   ip_version);
      }
    }
  } catch (const std::exception &ex) {
    fmt::print(stderr, "Dispatcher main loop stopped: {}\n", ex.what());
  }
}
