#include "core/logging/logger.hpp"

#include "core/dispatcher.hpp"
#include "core/handlers/icmp_handler.hpp"
#include "core/handlers/tcp_handler.hpp"
#include "core/proxy/clients/socks5/client.hpp"
#include "device/tun.h"
#include <filesystem>

int main(int argc, char *argv[]) {
#ifdef DEBUG
  setvbuf(stdout, nullptr, _IONBF, 0);
#endif

  try {
    constexpr auto TUN_NAME = "tun0";
    constexpr auto TUN_IP4 = "192.168.69.69";
    constexpr auto TUN_IP4_MASK = "255.255.255.0";
    constexpr auto TUN_IP6 = "fd69::69";
    constexpr auto TUN_IP6_PREFIX_MASK = 64;

    constexpr auto ICMP_HANDLER_NUMBER = 1;
    constexpr auto TCP_HANDLER_NUMBER = 6;

    // --- 1. Check for and Parse Command-Line Argument ---
    // Initialize logging first
    stack::log::Logger::init();

    if (argc < 2) {
      // Extract just the executable name from the full path using std::filesystem
      std::filesystem::path exe_path(argv[0]);
      std::string exe_name = exe_path.filename().string();
      LOG_ERROR("Usage: {} <socks5_ip:port>", exe_name);
      return 1;
    }

    std::string socks_arg(argv[1]);
    size_t colon_pos = socks_arg.find_last_of(':');
    if (colon_pos == std::string::npos) {
      throw std::runtime_error(
          "Invalid SOCKS endpoint format. Expected ip:port");
    }

    std::string socks_ip_str = socks_arg.substr(0, colon_pos);
    std::string socks_port_str = socks_arg.substr(colon_pos + 1);

    // --- 2. Convert Strings and Create the Endpoint ---
    boost::system::error_code ec;
    auto socks_ip = asio::ip::make_address(socks_ip_str, ec);
    if (ec) {
      throw std::runtime_error("Invalid SOCKS IP address: " + ec.message());
    }

    unsigned long socks_port_ul = std::stoul(socks_port_str);
    if (socks_port_ul > 65535) {
      throw std::runtime_error("Invalid SOCKS port number: " +
                               std::to_string(socks_port_ul));
    }

    asio::ip::tcp::endpoint socks_endpoint(
        socks_ip, static_cast<uint16_t>(socks_port_ul));

    LOG_INFO("Proxying through SOCKS5 server at {}:{}",
             socks_endpoint.address().to_string(), socks_endpoint.port());

    asio::io_context io_context;

    const auto tun_device =
        stack::device::create_tun(io_context, TUN_NAME, TUN_IP4, TUN_IP4_MASK,
                                  TUN_IP6, TUN_IP6_PREFIX_MASK);

    IPPacketChannel incoming_channel(io_context, 128);
    IPPacketChannel outgoing_channel(io_context, 128);

    Dispatcher dispatcher(io_context.get_executor(), incoming_channel);
    auto icmp_handler = std::make_unique<stack::handlers::IcmpHandler>(
        io_context.get_executor(), outgoing_channel, TUN_IP4, TUN_IP6);

    // Use the type alias to declare your factory lambda
    stack::handlers::ProxyClientFactory socks5_factory =
        [&]() -> std::unique_ptr<stack::proxy::clients::IProxyClient> {
      return std::make_unique<stack::proxy::clients::Socks5Client>(
          io_context.get_executor(), socks_endpoint);
    };

    auto tcp_handler = std::make_unique<stack::handlers::TcpHandler>(
        io_context.get_executor(), outgoing_channel,
        socks5_factory // Pass the factory
    );

    // Register it to handle (ICMP).
    dispatcher.register_handler(ICMP_HANDLER_NUMBER, std::move(icmp_handler));
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
