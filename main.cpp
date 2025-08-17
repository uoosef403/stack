#include <fmt/core.h>

#include "tun_device.hpp"
#include "core/dispatcher.hpp"

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);

    constexpr auto TUN_NAME = "tun0";
    constexpr auto TUN_IP = "192.168.7.1";
    constexpr auto TUN_NETMASK = "255.255.255.0";

    try {
        asio::io_context io_context;

        TunDevice tun_device(io_context, TUN_NAME, TUN_IP, TUN_NETMASK);

        IPPacketChannel incoming_channel(io_context, 128);
        IPPacketChannel outgoing_channel(io_context, 128);

        Dispatcher dispatcher(io_context.get_executor(), incoming_channel, outgoing_channel);

        asio::co_spawn(io_context, tun_device.run_reader(incoming_channel), asio::detached);
        asio::co_spawn(io_context, tun_device.run_writer(outgoing_channel), asio::detached);
        asio::co_spawn(io_context, dispatcher.run(), asio::detached);

        asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto){ io_context.stop(); });

        fmt::print("\n✅ Stack is running on {}\n", TUN_IP);
        fmt::print("  Press Ctrl+C to exit.\n");

        io_context.run();

       fmt::print("Shutting down.\n");
    } catch (const std::exception& e) {
        fmt::print("❌ Exception in main: {}\n", e.what());
        return 1;
    }
    return 0;
}
