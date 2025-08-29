#include "dispatcher.hpp"

#include <fmt/core.h>
#include <tins/tins.h>

Dispatcher::Dispatcher(const asio::any_io_executor& executor,
                       IPPacketChannel& incoming_channel)
    : executor_(executor),
      incoming_channel_(incoming_channel)
{
}

// Destructor implementation
Dispatcher::~Dispatcher() = default;

asio::awaitable<void> Dispatcher::run()
{
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
                        reinterpret_cast<const uint8_t*>(packet_ptr->data()),
                        static_cast<uint32_t>(packet_ptr->size())
                    );

                    // 2. Find the appropriate handler.
                    if (auto it = handlers_.find(ip_packet_ptr->protocol()); it != handlers_.end()) {
                        // 3. Move the unique_ptr to the handler, transferring ownership.
                        it->second->handle_ipv4(std::move(ip_packet_ptr));
                    } else {
                        fmt::println(
                            stderr,
                            "Dispatcher: No handler for protocol number {}",
                            ip_packet_ptr->protocol()
                        );
                    }
                } catch (const Tins::malformed_packet& ex) {
                    fmt::print(stderr, "   ERROR: libtins rejected IPv4 packet: {}\n", ex.what());
                }
            } else if (ip_version == 6) {
                try {
                    // Do the same for IPv6
                    auto ipv6_packet_ptr = std::make_unique<Tins::IPv6>(
                        reinterpret_cast<const uint8_t*>(packet_ptr->data()),
                        static_cast<uint32_t>(packet_ptr->size())
                    );

                    if (auto it = handlers_.find(ipv6_packet_ptr->next_header()); it != handlers_.end()) {
                        it->second->handle_ipv6(std::move(ipv6_packet_ptr));
                    } else {
                        fmt::println(
                            stderr,
                            "Dispatcher: No handler for protocol number {}",
                            ipv6_packet_ptr->next_header()
                        );
                    }
                } catch (const Tins::malformed_packet& ex) {
                    fmt::print(stderr, "   ERROR: libtins rejected IPv6 packet: {}\n", ex.what());
                }
            }
            // Note: The raw buffer 'packet_ptr' goes out of scope here and its memory is freed.
        }
    } catch (const std::exception& ex) {
        fmt::print(stderr, "Dispatcher main loop stopped: {}\n", ex.what());
    }
}

void Dispatcher::register_handler(uint8_t protocol_number, std::unique_ptr<IProtocolHandler> handler) {
    handlers_[protocol_number] = std::move(handler);
}