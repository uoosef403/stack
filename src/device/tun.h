#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <string>

#include "definitions.hpp"

namespace stack::device {
    /**
     * @brief An abstract interface for a platform-specific TUN virtual network
     * device.
     */
    class Tun {
    public:
        virtual ~Tun() = default;

        virtual asio::awaitable<void>
        run_reader(IPPacketChannel &incoming_channel) = 0;

        virtual asio::awaitable<void>
        run_writer(IPPacketChannel &outgoing_channel) = 0;
    };

    /**
     * @brief Factory function to create and configure a new platform-specific TUN
     * device.
     * @param io_context The asio::io_context to attach the device's async
     * operations to.
     * @param uri The name of the device (e.g., "stack0") or a URI for an existing
     * file descriptor (e.g., "tun://fd/15").
     * @param tun_ip4 The tun ip4, like 192.168.69.69
     * @param tun_ip4_mask The tun ip4 mask, like 255.255.255.0
     * @param tun_ip6 The tun ip6 mask, like fd69::69
     * @param tun_ip6_prefix_mask something like 64
     * @return A unique_ptr to a Tun device, or nullptr on failure.
     */
    std::unique_ptr<Tun> create_tun(
        asio::io_context &io_context,
        const std::string &uri,
        const std::string &tun_ip4,
        const std::string &tun_ip4_mask,
        const std::string &tun_ip6,
        const uint8_t tun_ip6_prefix_mask
    );
} // namespace stack::device
