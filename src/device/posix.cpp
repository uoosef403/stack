#if defined(__linux__) || defined(__ANDROID__)

#include "posix.h"
#include <fmt/core.h>

#include <cstring>
#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace stack::device
{
    // Helper RAII wrapper for the temporary socket used for ioctl calls.
    // This guarantees the socket is closed even if an exception is thrown.
    struct IoctlSocket
    {
        int fd = -1;

        IoctlSocket()
        {
            fd = socket(AF_INET, SOCK_DGRAM, 0);
            if (fd < 0)
            {
                throw std::system_error(errno, std::generic_category(),
                                        "Failed to create socket for ioctl");
            }
        }

        ~IoctlSocket()
        {
            if (fd >= 0)
            {
                close(fd);
            }
        }
    };

    PosixTun::PosixTun(asio::io_context& io_context) : asio_descriptor_(io_context)
    {
    }

    PosixTun::~PosixTun()
    {
        if (fd_ >= 0) {
            fmt::print("Closing TUN device: {}\n", name_);
            if (asio_descriptor_.is_open())
            {
                asio_descriptor_.close();
            }

            if (!from_fd_) // if the device wasnt made by me im not responsible to bring it done
            {
                close(fd_);
                // Attempt to bring the interface down, but don't throw from destructor.
                try {
                    IoctlSocket sock;
                    ifreq ifr{};
                    std::strncpy(ifr.ifr_name, name_.c_str(), IFNAMSIZ - 1);

                    if (ioctl(sock.fd, SIOCGIFFLAGS, &ifr) == 0) {
                        ifr.ifr_flags &= ~IFF_UP;
                        ioctl(sock.fd, SIOCSIFFLAGS, &ifr);
                    }
                } catch (const std::exception &e) {
                    fmt::print("Error during TUN device cleanup: {}\n", e.what());
                }
            }
        }
    }

    asio::awaitable<void> PosixTun::run_reader(IPPacketChannel& incoming_channel)
    {
        try
        {
            while (true)
            {
                std::vector<uint8_t> buffer(2048);

                auto [ec, len] = co_await asio_descriptor_.async_read_some(
                    asio::buffer(buffer), asio::as_tuple(asio::use_awaitable));

                // After unpacking, check the error code.
                if (ec)
                {
                    // Throwing here will be caught by the catch block below.
                    throw boost::system::system_error(ec);
                }

                buffer.resize(len);
                co_await incoming_channel.async_send({}, std::move(buffer));
            }
        }
        catch (const std::exception& e)
        {
            if (errno != EBADF)
            {
                fmt::print(stderr, "TUN reader stopped: {}\n", e.what());
            }
            incoming_channel.close();
        }
    }

    asio::awaitable<void> PosixTun::run_writer(IPPacketChannel& outgoing_channel)
    {
        try
        {
            while (true)
            {
                auto [ec, buffer] = co_await outgoing_channel.async_receive(
                    asio::as_tuple(asio::use_awaitable));

                // Check the error code.
                if (ec)
                {
                    // Channel closed or other error, exit the coroutine.
                    co_return;
                }

                co_await asio::async_write(asio_descriptor_, asio::buffer(buffer),
                                           asio::use_awaitable);
            }
        }
        catch (const std::exception& e)
        {
            if (errno != EBADF)
            {
                fmt::print(stderr, "TUN writer stopped: {}\n", e.what());
            }
        }
    }

    // The assign method remains the same.
    bool PosixTun::assign(const int fd)
    {
        if (fd < 0)
        {
            return false;
        }

        asio_descriptor_.assign(fd);

        return true;
    }

    bool PosixTun::open(const std::string& name)
    {
        from_fd_ = false;

        ifreq ifr{};
        if ((fd_ = ::open("/dev/net/tun", O_RDWR | O_NONBLOCK)) < 0)
        {
            fmt::println("Failed to open /dev/net/tun: {}", strerror(errno));
            return false;
        }

        std::memset(&ifr, 0, sizeof(ifr));
        ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
        std::strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);

        if (ioctl(fd_, TUNSETIFF, static_cast<void*>(&ifr)) < 0)
        {
            fmt::println("ioctl(TUNSETIFF) failed for device '{}': {}", name,
                         strerror(errno));
            ::close(fd_);
            return false;
        }


        name_ = ifr.ifr_name;

        fmt::print("Created TUN device: {}\n", name_);

        return this->assign(fd_); // Call assign internally
    }

    std::unique_ptr<Tun> PosixTun::create(asio::io_context& io_context,
                                          const std::string& name)
    {
        // This is now much cleaner.
        auto tun = std::unique_ptr<PosixTun>(new PosixTun(io_context));

        if (!tun->open(name))
        {
            return nullptr;
        }

        fmt::println("Successfully created TUN device '{}'", name);
        return tun;
    }

    std::unique_ptr<Tun> PosixTun::from_fd(asio::io_context& io_context, int fd)
    {
        auto tun = std::unique_ptr<PosixTun>(new PosixTun(io_context));
        if (!tun->assign(fd))
        {
            fmt::println("Failed to assign existing file descriptor {}.", fd);
            return nullptr;
        }
        fmt::println("Wrapping existing file descriptor {} as TUN device.", fd);
        return tun;
    }
} // namespace stack::device

#endif
