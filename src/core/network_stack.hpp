#pragma once

#include "definitions.hpp"
#include "device/tun.h"
#include "config/config.hpp"
#include "dispatcher.hpp"
#include "handlers/icmp_handler.hpp"
#include "handlers/tcp_handler.hpp"
#include <boost/asio.hpp>
#include <memory>


namespace stack::core {

class NetworkStack {
public:
  explicit NetworkStack(const Config &config);

  // Non-copyable, non-moveable for simplicity
  NetworkStack(const NetworkStack &) = delete;
  NetworkStack &operator=(const NetworkStack &) = delete;
  NetworkStack(NetworkStack &&) = delete;
  NetworkStack &operator=(NetworkStack &&) = delete;

  ~NetworkStack() = default;

  // Initialize all components
  void initialize();

  // Start the network stack (blocking call)
  void run();

  // Stop the network stack
  void stop();

private:
  void initialize_device();
  void initialize_handlers();
  void setup_signal_handlers();

  // Configuration references
  const NetworkConfig &network_config_;
  const ProxyConfig &proxy_config_;
  const TcpConfig &tcp_config_;
  const UdpConfig &udp_config_;
  const IcmpConfig &icmp_config_;

  // Core components
  asio::io_context io_context_;
  std::unique_ptr<stack::device::Tun> tun_device_;
  std::unique_ptr<IPPacketChannel> incoming_channel_;
  std::unique_ptr<IPPacketChannel> outgoing_channel_;
  std::unique_ptr<Dispatcher> dispatcher_;
  std::unique_ptr<asio::signal_set> signals_;

  // Handlers
  std::unique_ptr<handlers::IcmpHandler> icmp_handler_;
  std::unique_ptr<handlers::TcpHandler> tcp_handler_;

  // Proxy factory
  stack::handlers::ProxyClientFactory proxy_factory_;
};

} // namespace stack::core
