#include "tun_device.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <fmt/base.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <system_error> // For std::system_error
#include <unistd.h>

// Helper RAII wrapper for the temporary socket used for ioctl calls.
// This guarantees the socket is closed even if an exception is thrown.
struct IoctlSocket {
  int fd = -1;

  IoctlSocket() {
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
      throw std::system_error(errno, std::generic_category(),
                              "Failed to create socket for ioctl");
    }
  }

  ~IoctlSocket() {
    if (fd >= 0) {
      close(fd);
    }
  }
};

// Helper to create a descriptive exception with the system error.
static std::runtime_error make_sys_error(const std::string &msg) {
  return std::runtime_error(msg + ": " + strerror(errno));
}

TunDevice::TunDevice(asio::io_context &ioc, const std::string &device_name,
                     const std::string &ip_addr, const std::string &netmask)
    : asio_descriptor_(ioc) {
  try {
    create_device(device_name);
    asio_descriptor_.assign(fd_);
    set_ip(ip_addr);
    set_netmask(netmask);
    set_if_up();
  } catch (...) {
    // If anything fails during construction, ensure the main fd is closed.
    if (fd_ >= 0) {
      close(fd_);
    }
    // Re-throw the original exception.
    throw;
  }
}

TunDevice::~TunDevice() {
  if (fd_ >= 0) {
    fmt::print("Closing TUN device: {}\n", name_);
    close(fd_);
  }

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

void TunDevice::create_device(const std::string &device_name) {
  fd_ = open("/dev/net/tun", O_RDWR);
  if (fd_ < 0) {
    throw make_sys_error("Failed to open /dev/net/tun");
  }

  ifreq ifr{};
  ifr.ifr_flags = IFF_TUN | IFF_NO_PI; // TUN device, no packet info.

  if (!device_name.empty()) {
    std::strncpy(ifr.ifr_name, device_name.c_str(), IFNAMSIZ - 1);
  }

  if (ioctl(fd_, TUNSETIFF, &ifr) < 0) {
    throw make_sys_error("Failed to create TUN device via ioctl(TUNSETIFF)");
  }

  name_ = ifr.ifr_name;

  fmt::print("Created TUN device: {}\n", name_);
}

void TunDevice::set_ip(const std::string &ip_addr) const {
  IoctlSocket sock;
  ifreq ifr{};
  std::strncpy(ifr.ifr_name, name_.c_str(), IFNAMSIZ - 1);

  sockaddr_in *addr = reinterpret_cast<sockaddr_in *>(&ifr.ifr_addr);
  addr->sin_family = AF_INET;
  if (inet_pton(AF_INET, ip_addr.c_str(), &addr->sin_addr) != 1) {
    throw std::runtime_error("Invalid IP address format for: " + ip_addr);
  }

  if (ioctl(sock.fd, SIOCSIFADDR, &ifr) < 0) {
    throw make_sys_error("Failed to set IP address");
  }
}

void TunDevice::set_netmask(const std::string &netmask) const {
  IoctlSocket sock;
  ifreq ifr{};
  std::strncpy(ifr.ifr_name, name_.c_str(), IFNAMSIZ - 1);

  sockaddr_in *addr = reinterpret_cast<sockaddr_in *>(&ifr.ifr_addr);
  addr->sin_family = AF_INET;
  if (inet_pton(AF_INET, netmask.c_str(), &addr->sin_addr) != 1) {
    throw std::runtime_error("Invalid netmask format for: " + netmask);
  }

  // NOTE: SIOCSIFNETMASK also uses the ifr_addr field, not a separate
  // ifr_netmask.
  if (ioctl(sock.fd, SIOCSIFNETMASK, &ifr) < 0) {
    throw make_sys_error("Failed to set netmask");
  }
}

void TunDevice::set_if_up() const {
  IoctlSocket sock;
  ifreq ifr{};
  std::strncpy(ifr.ifr_name, name_.c_str(), IFNAMSIZ - 1);

  if (ioctl(sock.fd, SIOCGIFFLAGS, &ifr) < 0) {
    throw make_sys_error("Failed to get interface flags");
  }

  ifr.ifr_flags |= (IFF_UP | IFF_RUNNING);

  if (ioctl(sock.fd, SIOCSIFFLAGS, &ifr) < 0) {
    throw make_sys_error("Failed to bring interface up");
  }
}

asio::awaitable<void> TunDevice::run_reader(IPPacketChannel &incoming_channel) {
  try {
    while (true) {
      std::vector<uint8_t> buffer(2048);

      auto [ec, len] = co_await asio_descriptor_.async_read_some(
          asio::buffer(buffer), asio::as_tuple(asio::use_awaitable));

      // After unpacking, check the error code.
      if (ec) {
        // Throwing here will be caught by the catch block below.
        throw boost::system::system_error(ec);
      }

      buffer.resize(len);
      co_await incoming_channel.async_send({}, std::move(buffer));
    }
  } catch (const std::exception &e) {
    if (errno != EBADF) {
      fmt::print(stderr, "TUN reader stopped: {}\n", e.what());
    }
    incoming_channel.close();
  }
}

asio::awaitable<void> TunDevice::run_writer(IPPacketChannel &outgoing_channel) {
  try {
    while (true) {
      auto [ec, buffer] = co_await outgoing_channel.async_receive(
          asio::as_tuple(asio::use_awaitable));

      // Check the error code.
      if (ec) {
        // Channel closed or other error, exit the coroutine.
        co_return;
      }

      co_await asio::async_write(asio_descriptor_, asio::buffer(buffer),
                                 asio::use_awaitable);
    }
  } catch (const std::exception &e) {
    if (errno != EBADF) {
      fmt::print(stderr, "TUN writer stopped: {}\n", e.what());
    }
  }
}

int TunDevice::get_fd() const { return fd_; }

std::string_view TunDevice::get_name() const { return name_; }
