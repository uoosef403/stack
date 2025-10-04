#pragma once

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/signal_set.hpp>


namespace asio = boost::asio;

// A smart pointer to a raw packet data buffer.
using PacketBufferPtr = std::unique_ptr<std::vector<uint8_t>>;

// The channel now transports the smart pointer, not the vector itself.
using IPPacketChannel = asio::experimental::channel<void(
    boost::system::error_code, PacketBufferPtr)>;

namespace stack::constants {

// IP Protocol Numbers (from RFC 790)
constexpr uint8_t ICMP_PROTOCOL = 1;
constexpr uint8_t TCP_PROTOCOL = 6;
constexpr uint8_t UDP_PROTOCOL = 17;

// Default channel buffer sizes
constexpr size_t DEFAULT_CHANNEL_BUFFER_SIZE = 128;

// Default IPv6 values (temporary until proper IPv6 config is implemented)
constexpr const char *DEFAULT_IPV6_ADDRESS = "fd69::69";
constexpr uint8_t DEFAULT_IPV6_PREFIX = 64;

} // namespace stack::constants

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
