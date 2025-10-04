#include "config.hpp"
#include "core/dispatcher.hpp"
#include "core/handlers/icmp_handler.hpp"
#include "core/handlers/tcp_handler.hpp"
#include "core/logging/logger.hpp"
#include "core/proxy/clients/socks5/client.hpp"
#include "device/tun.h"


int main(int argc, char *argv[]) {
#ifdef DEBUG
  setvbuf(stdout, nullptr, _IONBF, 0);
#endif

  try {
    constexpr auto ICMP_HANDLER_NUMBER = 1;
    constexpr auto TCP_HANDLER_NUMBER = 6;

    // --- 1. Initialize Configuration ---
    Config &config = Config::getInstance();

    // Parse command line arguments first
    if (!config.parseCommandLine(argc, argv)) {
      return 1; // Help was shown or error occurred
    }

    // Initialize logging with configured level
    stack::log::Logger::init();

    const auto &net_config = config.getNetworkConfig();
    const auto &proxy_config = config.getProxyConfig();
    const auto &icmp_config = config.getIcmpConfig();

    LOG_INFO("Starting Network Stack");
    LOG_INFO("Interface: {} ({})", net_config.interface_name,
             net_config.interface_ip);

    if (proxy_config.enable_socks5) {
      LOG_INFO("SOCKS5 Proxy: {}:{}", proxy_config.socks5_host,
               proxy_config.socks5_port);
    } else {
      LOG_WARN("SOCKS5 proxy is disabled - direct connections will be used");
    }

    // --- 2. Create SOCKS5 Endpoint from Configuration ---
    asio::ip::tcp::endpoint socks_endpoint;
    if (proxy_config.enable_socks5) {
      boost::system::error_code ec;
      auto socks_ip = asio::ip::make_address(proxy_config.socks5_host, ec);
      if (ec) {
        throw std::runtime_error(
            "Invalid SOCKS5 IP address in configuration: " + ec.message());
      }
      socks_endpoint =
          asio::ip::tcp::endpoint(socks_ip, proxy_config.socks5_port);
    }

    asio::io_context io_context;

    // --- 3. Create TUN Device with Configuration ---
    const auto tun_device = stack::device::create_tun(
        io_context, net_config.interface_name.c_str(),
        net_config.interface_ip.c_str(), net_config.interface_netmask.c_str(),
        "fd69::69", // TODO: Add IPv6 configuration
        64          // TODO: Add IPv6 prefix configuration
    );

    IPPacketChannel incoming_channel(io_context, 128);
    IPPacketChannel outgoing_channel(io_context, 128);

    Dispatcher dispatcher(io_context.get_executor(), incoming_channel);

    // --- 4. Initialize Protocol Handlers with Configuration ---
    std::unique_ptr<stack::handlers::IcmpHandler> icmp_handler;
    if (icmp_config.enable) {
      icmp_handler = std::make_unique<stack::handlers::IcmpHandler>(
          io_context.get_executor(), outgoing_channel,
          net_config.interface_ip.c_str(), "fd69::69");
      dispatcher.register_handler(ICMP_HANDLER_NUMBER, std::move(icmp_handler));
      LOG_INFO("ICMP handler enabled");
    } else {
      LOG_INFO("ICMP handler disabled");
    }

    // Create proxy client factory based on configuration
    stack::handlers::ProxyClientFactory proxy_factory;
    if (proxy_config.enable_socks5) {
      proxy_factory = [&, socks_endpoint]()
          -> std::unique_ptr<stack::proxy::clients::IProxyClient> {
        return std::make_unique<stack::proxy::clients::Socks5Client>(
            io_context.get_executor(), socks_endpoint);
      };
    } else {
      // TODO: Implement direct connection factory when SOCKS5 is disabled
      LOG_WARN(
          "Direct connections not yet implemented, using null proxy factory");
      proxy_factory =
          []() -> std::unique_ptr<stack::proxy::clients::IProxyClient> {
        return nullptr;
      };
    }

    auto tcp_handler = std::make_unique<stack::handlers::TcpHandler>(
        io_context.get_executor(), outgoing_channel, proxy_factory);

    dispatcher.register_handler(TCP_HANDLER_NUMBER, std::move(tcp_handler));

    asio::co_spawn(io_context, tun_device->run_reader(incoming_channel),
                   asio::detached);
    asio::co_spawn(io_context, tun_device->run_writer(outgoing_channel),
                   asio::detached);
    asio::co_spawn(io_context, dispatcher.run(), asio::detached);

    asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto) { io_context.stop(); });

    LOG_INFO("Stack is running - Press Ctrl+C to exit");

    io_context.run();

    LOG_INFO("Shutting down");
  } catch (const std::exception &e) {
    LOG_ERROR("Exception in main: {}", e.what());
    return 1;
  }
  return 0;
}
