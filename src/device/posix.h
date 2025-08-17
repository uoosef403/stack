#pragma once
#if defined(__linux__) || defined(__ANDROID__)

#include "tun.h"
#include "asio/posix/stream_descriptor.hpp"

namespace stack::device {

    /**
     * @brief POSIX-specific implementation of the Tun interface (for Linux, Android).
     */
    class PosixTun final : public Tun {
    public:
        ~PosixTun() override;

        PosixTun(const PosixTun&) = delete;
        PosixTun& operator=(const PosixTun&) = delete;
        PosixTun(PosixTun&&) = delete;
        PosixTun& operator=(PosixTun&&) = delete;

        asio::awaitable<size_t> async_read_some(const asio::mutable_buffer& buffer) override;
        asio::awaitable<size_t> async_write_some(const asio::const_buffer& buffer) override;

        /**
         * @brief Factory method to create a new TUN device.
         */
        static std::unique_ptr<Tun> create(asio::io_context& io_context, const std::string& name);

        /**
         * @brief Factory method to wrap an existing TUN file descriptor.
         */
        static std::unique_ptr<Tun> from_fd(asio::io_context& io_context, int fd);

    private:
        explicit PosixTun(asio::io_context& io_context);
        bool open(const std::string& name);
        bool assign(int fd);

        asio::posix::stream_descriptor m_descriptor;
    };

} // namespace stack::device

#endif