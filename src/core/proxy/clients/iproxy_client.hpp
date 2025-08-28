#pragma once

#include "core/handlers/tcp/pcb.hpp"
#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace asio = boost::asio;

namespace stack::proxy::clients {

class IProxyClient {
public:
  virtual ~IProxyClient() = default;

  // Establishes a connection to the destination via the proxy.
  // Returns a connected and ready-to-use socket on success.
  virtual asio::awaitable<asio::ip::tcp::socket>
  connect(const stack::tcp::four_tuple &destination) = 0;
};

} // namespace proxy::clients