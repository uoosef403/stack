#if defined(__linux__) || defined(__ANDROID__)

#include "posix.h"
#include <fmt/core.h>

#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

namespace stack::device {

    PosixTun::PosixTun(asio::io_context& io_context) : m_descriptor(io_context) {}

    PosixTun::~PosixTun() {
        if (m_descriptor.is_open()) {
            m_descriptor.close();
        }
    }

    asio::awaitable<size_t> PosixTun::async_read_some(const asio::mutable_buffer& buffer) {
        // co_await the native Asio operation and get the result as a tuple.
        auto [ec, bytes_read] = co_await m_descriptor.async_read_some(buffer, asio::as_tuple(asio::use_awaitable));

        // Propagate errors via exceptions, the standard for coroutines.
        if (ec) {
            throw boost::system::system_error(ec);
        }

        co_return bytes_read;
    }

    asio::awaitable<size_t> PosixTun::async_write_some(const asio::const_buffer& buffer) {
        auto [ec, bytes_written] = co_await m_descriptor.async_write_some(buffer, asio::as_tuple(asio::use_awaitable));

        if (ec) {
            throw boost::system::system_error(ec);
        }

        co_return bytes_written;
    }

    // The assign method remains the same.
    bool PosixTun::assign(int fd) {
        if (fd < 0) return false;

        asio::error_code ec;
        m_descriptor.assign(fd, ec);

        if (ec) {
            fmt::println("Failed to assign existing file descriptor {}: {}", fd, ec.message());
            return false;
        }

        return true;
    }

    bool PosixTun::open(const std::string& name) {
        ifreq ifr{};
        int fd;

        if ((fd = ::open("/dev/net/tun", O_RDWR | O_NONBLOCK)) < 0) {
            fmt::println("Failed to open /dev/net/tun: {}", strerror(errno));
            return false;
        }

        std::memset(&ifr, 0, sizeof(ifr));
        ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
        std::strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);

        if (ioctl(fd, TUNSETIFF, static_cast<void*>(&ifr)) < 0) {
            fmt::println("ioctl(TUNSETIFF) failed for device '{}': {}", name, strerror(errno));
            ::close(fd);
            return false;
        }

        return this->assign(fd); // Call assign internally
    }

    std::unique_ptr<Tun> PosixTun::create(asio::io_context& io_context, const std::string& name) {
        // This is now much cleaner.
        auto tun = std::unique_ptr<PosixTun>(new PosixTun(io_context));

        if (!tun->open(name)) {
            return nullptr;
        }

        fmt::println("Successfully created TUN device '{}'", name);
        return tun;
    }

    std::unique_ptr<Tun> PosixTun::from_fd(asio::io_context& io_context, int fd) {
        auto tun = std::unique_ptr<PosixTun>(new PosixTun(io_context));
        if (!tun->assign(fd)) {
            fmt::println("Failed to assign existing file descriptor {}.", fd);
            return nullptr;
        }
        fmt::println("Wrapping existing file descriptor {} as TUN device.", fd);
        return tun;
    }

} // namespace stack::device

#endif