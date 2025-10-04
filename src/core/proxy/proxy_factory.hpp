#pragma once

#include "core/config/config.hpp"
#include "core/handlers/tcp_handler.hpp"
#include <boost/asio.hpp>

namespace stack::proxy {

class ProxyFactory {
public:
  static stack::handlers::ProxyClientFactory
  create_factory(const ProxyConfig &config, asio::any_io_executor executor);

private:
  static asio::ip::tcp::endpoint
  create_socks5_endpoint(const ProxyConfig &config);
};

} // namespace stack::proxy
