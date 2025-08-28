#include <fmt/core.h>

#include "device/tun.h"
#include "core/dispatcher.hpp"
#include "core/handlers/icmp_handler.hpp"
#include "core/handlers/tcp_handler.hpp"

int main() {
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

        asio::io_context io_context;

        const auto tun_device = stack::device::create_tun(
            io_context,
            TUN_NAME,
            TUN_IP4,
            TUN_IP4_MASK,
            TUN_IP6,
            TUN_IP6_PREFIX_MASK
        );

        IPPacketChannel incoming_channel(io_context, 128);
        IPPacketChannel outgoing_channel(io_context, 128);

        Dispatcher dispatcher(io_context.get_executor(), incoming_channel, outgoing_channel);
        auto icmp_handler = std::make_unique<stack::handlers::IcmpHandler>(
            io_context.get_executor(),
            outgoing_channel,
            TUN_IP4,
            TUN_IP6
        );

        // Use the type alias to declare your factory lambda
        stack::handlers::ProxyClientFactory socks5_factory = [&]() -> std::unique_ptr<stack::proxy::clients::IProxyClient> {
            return nullptr;
        };

        auto tcp_handler = std::make_unique<stack::handlers::TcpHandler>(
            io_context.get_executor(),
            outgoing_channel,
            socks5_factory // Pass the factory
        );

        // Register it to handle (ICMP).
        dispatcher.register_handler(ICMP_HANDLER_NUMBER, std::move(icmp_handler));
        dispatcher.register_handler(TCP_HANDLER_NUMBER, std::move(tcp_handler));

        asio::co_spawn(io_context, tun_device->run_reader(incoming_channel), asio::detached);
        asio::co_spawn(io_context, tun_device->run_writer(outgoing_channel), asio::detached);
        asio::co_spawn(io_context, dispatcher.run(), asio::detached);

        asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto) { io_context.stop(); });

        fmt::println("\n✅ Stack is running...");
        fmt::println("  Press Ctrl+C to exit.");

        io_context.run();

        fmt::println("Shutting down.");
    } catch (const std::exception &e) {
        fmt::println("❌ Exception in main: {}", e.what());
        return 1;
    }
    return 0;
}
