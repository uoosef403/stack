#pragma once

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/experimental/channel.hpp>

namespace  asio = boost::asio;

// A smart pointer to a raw packet data buffer.
using PacketBufferPtr = std::unique_ptr<std::vector<uint8_t>>;

// The channel now transports the smart pointer, not the vector itself.
using IPPacketChannel = asio::experimental::channel<void(boost::system::error_code, PacketBufferPtr)>;

// Debugbreak helper function is used to trigger a breakpoint in the debugger.
inline void debug_break() {
#if defined(_WIN32)
    __debugbreak();
#elif defined(__unix__) || defined(__APPLE__)
    raise(SIGTRAP);
#else
#error "Platform not supported"
#endif
}