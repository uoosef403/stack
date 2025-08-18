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
        static std::unique_ptr<Tun> create(asio::io_context& io_context,
                                           const std::string& name);

        /**
         * @brief Factory method to wrap an existing TUN file descriptor.
         */
        static std::unique_ptr<Tun> from_fd(asio::io_context& io_context, int fd);

    private:
        explicit PosixTun(asio::io_context& io_context);
        bool open(const std::string& name);
        bool set_ip(const std::string& ip_addr) const;
        bool set_netmask(const std::string& netmask) const;
        bool set_if_up() const;
        bool assign(int fd);

        int fd_ = -1;
        std::string name_;
        bool from_fd_ = true;
        asio::posix::stream_descriptor asio_descriptor_;
    };
} // namespace stack::device

#endif
