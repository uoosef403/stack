#if defined(__linux__) || defined(__ANDROID__)

#include "posix.h"
#include <fmt/core.h>

#include <cstring>
#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <linux/rtnetlink.h>
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

    PosixTun::PosixTun(
        asio::io_context& io_context,
        const std::string& tun_ip4,
        const std::string& tun_ip4_mask,
        const std::string& tun_ip6,
        const uint8_t tun_ip6_prefix_mask
    ) : asio_descriptor_(io_context), m_tun_ip4(tun_ip4),
        m_tun_ip4_mask(tun_ip4_mask), m_tun_ip6(tun_ip6),
        m_tun_ip6_prefix_mask(tun_ip6_prefix_mask)
    {
    }

    PosixTun::~PosixTun()
    {
        if (fd_ >= 0)
        {
            fmt::println(stderr, "Closing TUN device: {}\n", name_);
            if (asio_descriptor_.is_open())
            {
                asio_descriptor_.close();
            }

            if (!from_fd_) // if the device wasnt made by me im not responsible to bring it done
            {
                close(fd_);
                // Attempt to bring the interface down, but don't throw from destructor.
                try
                {
                    IoctlSocket sock;
                    ifreq ifr{};
                    std::strncpy(ifr.ifr_name, name_.c_str(), IFNAMSIZ - 1);

                    if (ioctl(sock.fd, SIOCGIFFLAGS, &ifr) == 0)
                    {
                        ifr.ifr_flags &= ~IFF_UP;
                        ioctl(sock.fd, SIOCSIFFLAGS, &ifr);
                    }
                }
                catch (const std::exception& e)
                {
                    fmt::println(stderr, "Error during TUN device cleanup: {}\n", e.what());
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
                auto buffer_ptr = std::make_unique<std::vector<uint8_t>>(std::move(buffer));
                co_await incoming_channel.async_send({}, std::move(buffer_ptr));
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

                co_await asio::async_write(asio_descriptor_, asio::buffer(*buffer),
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
            fmt::println(stderr, "Failed to open /dev/net/tun: {}", strerror(errno));
            return false;
        }

        std::memset(&ifr, 0, sizeof(ifr));
        ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
        std::strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);

        if (ioctl(fd_, TUNSETIFF, static_cast<void*>(&ifr)) < 0)
        {
            fmt::println(stderr, "ioctl(TUNSETIFF) failed for device '{}': {}", name,
                         strerror(errno));
            ::close(fd_);
            return false;
        }

        name_ = ifr.ifr_name;

        fmt::println(stderr, "Created TUN device: {}\n", name_);

        if (!configure(m_tun_ip4, m_tun_ip4_mask,
                       m_tun_ip6, m_tun_ip6_prefix_mask))
        {
            fmt::println(stderr, "Failed to assign IPs to Wintun adapter, do this yourself");
            return false;
        }

        return assign(fd_); // Call assign internally
    }

    bool PosixTun::configure(const std::string& ip4,
                             const std::string& netmask,
                             const std::string& ip6,
                             uint8_t prefix_len) const
    {
        IoctlSocket sock;
        ifreq ifr{};
        std::strncpy(ifr.ifr_name, name_.c_str(), IFNAMSIZ - 1);

        // --- IPv4 address ---
        if (!ip4.empty())
        {
            auto* addr = reinterpret_cast<sockaddr_in*>(&ifr.ifr_addr);
            addr->sin_family = AF_INET;
            if (inet_pton(AF_INET, ip4.c_str(), &addr->sin_addr) != 1)
            {
                fmt::println(stderr, "Invalid IPv4 address: {}", ip4);
                return false;
            }
            if (ioctl(sock.fd, SIOCSIFADDR, &ifr) < 0)
            {
                fmt::println(stderr, "Failed to set IPv4 address");
                return false;
            }
        }

        // --- IPv4 netmask ---
        if (!netmask.empty())
        {
            auto* mask = reinterpret_cast<sockaddr_in*>(&ifr.ifr_netmask);
            mask->sin_family = AF_INET;
            if (inet_pton(AF_INET, netmask.c_str(), &mask->sin_addr) != 1)
            {
                fmt::println(stderr, "Invalid netmask: {}", netmask);
                return false;
            }
            if (ioctl(sock.fd, SIOCSIFNETMASK, &ifr) < 0)
            {
                fmt::println(stderr, "Failed to set netmask");
                return false;
            }
        }

        // --- Bring interface up ---
        if (ioctl(sock.fd, SIOCGIFFLAGS, &ifr) < 0)
        {
            fmt::println(stderr, "Failed to get interface flags");
            return false;
        }
        ifr.ifr_flags |= (IFF_UP | IFF_RUNNING);
        if (ioctl(sock.fd, SIOCSIFFLAGS, &ifr) < 0)
        {
            fmt::println(stderr, "Failed to bring interface up");
            return false;
        }

        // --- IPv6 configuration via netlink ---
        if (!ip6.empty())
        {
            int nl = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
            if (nl < 0)
            {
                fmt::println(stderr, "Failed to open netlink socket");
                return false;
            }

            struct
            {
                nlmsghdr nlh;
                ifaddrmsg ifa;
                char buf[256];
            } req{};

            req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(ifaddrmsg));
            req.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
            req.nlh.nlmsg_type = RTM_NEWADDR;

            req.ifa.ifa_family = AF_INET6;
            req.ifa.ifa_prefixlen = prefix_len;
            req.ifa.ifa_index = if_nametoindex(name_.c_str());
            req.ifa.ifa_scope = 0;

            // Add IFA_LOCAL (the IPv6 address itself)
            in6_addr in6{};
            if (inet_pton(AF_INET6, ip6.c_str(), &in6) != 1)
            {
                fmt::println(stderr, "Invalid IPv6 address: {}", ip6);
                close(nl);
                return false;
            }
            rtattr* rta = reinterpret_cast<struct rtattr*>(
                reinterpret_cast<char*>(&req) + NLMSG_ALIGN(req.nlh.nlmsg_len)
            );
            int rta_len = RTA_LENGTH(sizeof(in6));
            rta->rta_type = IFA_LOCAL;
            rta->rta_len = rta_len;
            memcpy(RTA_DATA(rta), &in6, sizeof(in6));
            req.nlh.nlmsg_len = NLMSG_ALIGN(req.nlh.nlmsg_len) + RTA_ALIGN(rta_len);

            if (send(nl, &req, req.nlh.nlmsg_len, 0) < 0)
            {
                fmt::println(stderr, "Failed to send IPv6 address via netlink: {}", strerror(errno));
                close(nl);
                return false;
            }
            close(nl);
        }

        return true;
    }

    std::unique_ptr<Tun> PosixTun::create(
        asio::io_context& io_context,
        const std::string& name,
        const std::string& tun_ip4,
        const std::string& tun_ip4_mask,
        const std::string& tun_ip6,
        const uint8_t tun_ip6_prefix_mask
    )
    {
        // This is now much cleaner.
        auto tun = std::unique_ptr<PosixTun>(
            new PosixTun(
                io_context,
                tun_ip4,
                tun_ip4_mask,
                tun_ip6,
                tun_ip6_prefix_mask
            )
        );

        if (!tun->open(name))
        {
            return nullptr;
        }

        fmt::println(stderr, "Successfully created TUN device '{}'", name);
        return tun;
    }

    std::unique_ptr<Tun> PosixTun::from_fd(
        asio::io_context& io_context,
        int fd,
        const std::string& tun_ip4,
        const std::string& tun_ip4_mask,
        const std::string& tun_ip6,
        const uint8_t tun_ip6_prefix_mask
    )
    {
        auto tun = std::unique_ptr<PosixTun>(
            new PosixTun(
                io_context,
                tun_ip4,
                tun_ip4_mask,
                tun_ip6,
                tun_ip6_prefix_mask
            )
        );
        if (!tun->assign(fd))
        {
            fmt::println(stderr, "Failed to assign existing file descriptor {}.", fd);
            return nullptr;
        }
        fmt::println(stderr, "Wrapping existing file descriptor {} as TUN device.", fd);
        return tun;
    }
} // namespace stack::device

#endif
