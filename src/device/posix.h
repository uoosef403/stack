#pragma once
#if defined(__linux__) || defined(__ANDROID__)

#include "tun.h"

namespace stack::device
{
    /**
     * @brief POSIX-specific implementation of the Tun interface (for Linux,
     * Android).
     */
    class PosixTun final : public Tun
    {
    public:
        ~PosixTun() override;

        PosixTun(const PosixTun&) = delete;
        PosixTun& operator=(const PosixTun&) = delete;
        PosixTun(PosixTun&&) = delete;
        PosixTun& operator=(PosixTun&&) = delete;

        asio::awaitable<void> run_reader(IPPacketChannel& incoming_channel) override;
        asio::awaitable<void> run_writer(IPPacketChannel& outgoing_channel) override;

        /**
         * @brief Factory method to create a new TUN device.
         */
        static std::unique_ptr<Tun> create(
            asio::io_context& io_context,
            const std::string& name,
            const std::string& tun_ip4,
            const std::string& tun_ip4_mask,
            const std::string& tun_ip6,
            uint8_t tun_ip6_prefix_mask
        );

        /**
         * @brief Factory method to wrap an existing TUN file descriptor.
         */
        static std::unique_ptr<Tun> from_fd(
            asio::io_context& io_context,
            int fd,
            const std::string& tun_ip4,
            const std::string& tun_ip4_mask,
            const std::string& tun_ip6,
            uint8_t tun_ip6_prefix_mask
        );

    private:
        explicit PosixTun(
            asio::io_context& io_context,
            const std::string& tun_ip4,
            const std::string& tun_ip4_mask,
            const std::string& tun_ip6,
            uint8_t tun_ip6_prefix_mask
        );

        bool open(const std::string& name);

        bool configure(
            const std::string& ip4,
            const std::string& netmask,
            const std::string& ip6,
            uint8_t prefix_len
        ) const;

        bool assign(int fd);

        int fd_ = -1;
        std::string name_;
        bool from_fd_ = true;
        asio::posix::stream_descriptor asio_descriptor_;
        const std::string& m_tun_ip4;
        const std::string& m_tun_ip4_mask;
        const std::string& m_tun_ip6;
        const uint8_t m_tun_ip6_prefix_mask;
    };
} // namespace stack::device

#endif
