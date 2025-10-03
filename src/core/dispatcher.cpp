#include "dispatcher.hpp"

#include "core/logging/logger.hpp"
#include <tins/tins.h>

Dispatcher::Dispatcher(asio::any_io_executor io_executor,
                       IPPacketChannel &channel)
    : executor_(std::move(io_executor)), incoming_channel_(channel) {}

// Destructor implementation
Dispatcher::~Dispatcher() = default;

asio::awaitable<void> Dispatcher::run() {
  try {
    while (true) {
      auto [ec, packet_ptr] = co_await incoming_channel_.async_receive(
          asio::as_tuple(asio::use_awaitable));

      if (ec || !packet_ptr || packet_ptr->empty()) {
        if (ec == asio::experimental::channel_errc::channel_closed) {
          break; // Channel closed, exit gracefully
        }
        continue;
      }

      // Safe, bounds-checked access
      uint8_t ip_version = packet_ptr->at(0) >> 4;

      if (ip_version == 4) {
        try {
          // 1. Create the parsed packet on the heap, managed by a unique_ptr.
          auto ip_packet_ptr = std::make_unique<Tins::IP>(
              reinterpret_cast<const uint8_t *>(packet_ptr->data()),
              static_cast<uint32_t>(packet_ptr->size()));

          // 2. Find the appropriate handler.
          if (auto it = handlers_.find(ip_packet_ptr->protocol());
              it != handlers_.end()) {
            // 3. Move the unique_ptr to the handler, transferring ownership.
            it->second->handle_ipv4(std::move(ip_packet_ptr));
          } else {
            LOG_WARN("Dispatcher: No handler for IPv4 protocol number {}",
                     ip_packet_ptr->protocol());
          }
        } catch (const Tins::malformed_packet &ex) {
          LOG_ERROR("Libtins rejected IPv4 packet: {}", ex.what());
        }
      } else if (ip_version == 6) {
        try {
          // Do the same for IPv6
          auto ipv6_packet_ptr = std::make_unique<Tins::IPv6>(
              reinterpret_cast<const uint8_t *>(packet_ptr->data()),
              static_cast<uint32_t>(packet_ptr->size()));

          if (auto it = handlers_.find(ipv6_packet_ptr->next_header());
              it != handlers_.end()) {
            it->second->handle_ipv6(std::move(ipv6_packet_ptr));
          } else {
            LOG_WARN("Dispatcher: No handler for IPv6 protocol number {}",
                     ipv6_packet_ptr->next_header());
          }
        } catch (const Tins::malformed_packet &ex) {
          LOG_ERROR("Libtins rejected IPv6 packet: {}", ex.what());
        }
      }
      // Note: The raw buffer 'packet_ptr' goes out of scope here and its memory
      // is freed.
    }
  } catch (const std::exception &ex) {
    LOG_ERROR("Dispatcher main loop stopped: {}", ex.what());
  }
}

void Dispatcher::register_handler(uint8_t protocol_number,
                                  std::unique_ptr<IProtocolHandler> handler) {
  handlers_[protocol_number] = std::move(handler);
}
