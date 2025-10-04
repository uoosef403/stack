#include "network_stack.hpp"
#include "definitions.hpp"
#include "core/proxy/proxy_factory.hpp"
#include "logging/logger.hpp"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <csignal>


namespace stack::core {

NetworkStack::NetworkStack(const Config &config)
    : network_config_(config.get_network_config()),
      proxy_config_(config.get_proxy_config()),
      tcp_config_(config.get_tcp_config()),
      udp_config_(config.get_udp_config()),
      icmp_config_(config.get_icmp_config()) {

  LOG_INFO("Initializing Network Stack");
  LOG_INFO("Interface: {} ({})", network_config_.interface_name,
           network_config_.interface_ip);

  if (proxy_config_.enable_socks5) {
    LOG_INFO("SOCKS5 Proxy: {}:{}", proxy_config_.socks5_host,
             proxy_config_.socks5_port);
  } else {
    LOG_WARN("SOCKS5 proxy is disabled - direct connections will be used");
  }
}

void NetworkStack::initialize() {
  LOG_INFO("Setting up network stack components...");

  initialize_device();
  initialize_handlers();
  setup_signal_handlers();

  LOG_INFO("Network stack initialization complete");
}

void NetworkStack::initialize_device() {
  LOG_DEBUG("Creating TUN device...");

  tun_device_ = stack::device::create_tun(
      io_context_, network_config_.interface_name, network_config_.interface_ip,
      network_config_.interface_netmask, network_config_.ipv6_address,
      network_config_.ipv6_prefix);

  // Create packet channels
  incoming_channel_ = std::make_unique<IPPacketChannel>(
      io_context_, constants::DEFAULT_CHANNEL_BUFFER_SIZE);
  outgoing_channel_ = std::make_unique<IPPacketChannel>(
      io_context_, constants::DEFAULT_CHANNEL_BUFFER_SIZE);

  // Create dispatcher
  dispatcher_ = std::make_unique<Dispatcher>(io_context_.get_executor(),
                                             *incoming_channel_);
}

void NetworkStack::initialize_handlers() {
  LOG_DEBUG("Initializing protocol handlers...");

  // Initialize ICMP handler if enabled
  if (icmp_config_.enable) {
    icmp_handler_ = std::make_unique<handlers::IcmpHandler>(
        io_context_.get_executor(), *outgoing_channel_,
        network_config_.interface_ip.c_str(),
        network_config_.ipv6_address.c_str());
    dispatcher_->register_handler(constants::ICMP_PROTOCOL,
                                  std::move(icmp_handler_));
    LOG_INFO("ICMP handler enabled");
  } else {
    LOG_INFO("ICMP handler disabled");
  }

  // Create proxy factory
  proxy_factory_ = stack::proxy::ProxyFactory::create_factory(
      proxy_config_, io_context_.get_executor());

  // Initialize TCP handler
  tcp_handler_ = std::make_unique<handlers::TcpHandler>(
      io_context_.get_executor(), *outgoing_channel_, proxy_factory_);
  dispatcher_->register_handler(constants::TCP_PROTOCOL,
                                std::move(tcp_handler_));
  LOG_INFO("TCP handler enabled");
}

void NetworkStack::setup_signal_handlers() {
  signals_ = std::make_unique<asio::signal_set>(io_context_, SIGINT, SIGTERM);
  signals_->async_wait([this](auto, auto) {
    LOG_INFO("Received shutdown signal, stopping...");
    stop();
  });
}

void NetworkStack::run() {
  LOG_INFO("Starting network stack components...");

  // Start async operations
  asio::co_spawn(io_context_, tun_device_->run_reader(*incoming_channel_),
                 asio::detached);
  asio::co_spawn(io_context_, tun_device_->run_writer(*outgoing_channel_),
                 asio::detached);
  asio::co_spawn(io_context_, dispatcher_->run(), asio::detached);

  LOG_INFO("Stack is running - Press Ctrl+C to exit");

  // Run the event loop (blocking)
  io_context_.run();

  LOG_INFO("Shutting down");
}

void NetworkStack::stop() { io_context_.stop(); }

} // namespace stack::core
