#pragma once

#include <string>
#include <string_view>

#include "definitions.hpp"

class TunDevice
{
public:
    // Constructor creates and fully configures the TUN device.
    TunDevice(asio::io_context& ioc,
              const std::string& device_name,
              const std::string& ip_addr,
              const std::string& netmask);

    // Destructor brings the interface down.
    ~TunDevice();

    // Deleted copy/move operations to prevent resource mismanagement.
    TunDevice(const TunDevice&) = delete;
    TunDevice& operator=(const TunDevice&) = delete;
    TunDevice(TunDevice&&) = delete;
    TunDevice& operator=(TunDevice&&) = delete;

    // Accessors.
    int get_fd() const;
    std::string_view get_name() const;
    asio::awaitable<void> run_reader(IPPacketChannel& incoming_channel);
    asio::awaitable<void> run_writer(IPPacketChannel& outgoing_channel);

private:
    // Private helpers to make the constructor clean and exception-safe.
    void create_device(const std::string& device_name);
    void set_ip(const std::string& ip_addr) const;
    void set_netmask(const std::string& netmask) const;
    void set_if_up() const;

    int fd_ = -1;
    std::string name_;

    asio::posix::stream_descriptor asio_descriptor_;
};