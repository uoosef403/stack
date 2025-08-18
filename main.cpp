#include <fmt/core.h>

#include "device/tun.h"
#include "core/dispatcher.hpp"

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);

    constexpr auto TUN_NAME = "tun0";

    try {
        asio::io_context io_context;

        const auto tun_device = stack::device::create_tun(io_context, TUN_NAME);

        IPPacketChannel incoming_channel(io_context, 128);
        IPPacketChannel outgoing_channel(io_context, 128);

        Dispatcher dispatcher(io_context.get_executor(), incoming_channel, outgoing_channel);

        asio::co_spawn(io_context, tun_device->run_reader(incoming_channel), asio::detached);
        asio::co_spawn(io_context, tun_device->run_writer(outgoing_channel), asio::detached);
        asio::co_spawn(io_context, dispatcher.run(), asio::detached);

        asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto){ io_context.stop(); });

        fmt::println("\n✅ Stack is running...");
        fmt::println("  Press Ctrl+C to exit.");

        io_context.run();

       fmt::println("Shutting down.");
    } catch (const std::exception& e) {
        fmt::println("❌ Exception in main: {}", e.what());
        return 1;
    }
    return 0;
}
