#include "proxy_factory.hpp"
#include "clients/socks5/client.hpp"
#include "core/logging/logger.hpp"
#include <boost/system/error_code.hpp>

namespace stack::proxy {

stack::handlers::ProxyClientFactory
ProxyFactory::create_factory(const ProxyConfig &config,
                             asio::any_io_executor executor) {

  if (config.enable_socks5) {
    LOG_INFO("Creating SOCKS5 proxy factory for {}:{}", config.socks5_host,
             config.socks5_port);

    auto endpoint = create_socks5_endpoint(config);

    return [executor = std::move(executor),
            endpoint]() -> std::unique_ptr<clients::IProxyClient> {
      return std::make_unique<clients::Socks5Client>(executor, endpoint);
    };
  }
  LOG_INFO("Creating direct connection factory (no proxy)");

  // For now, return null factory until direct connections are implemented
  // TODO: Implement DirectClient for non-proxy connections
  return []() -> std::unique_ptr<clients::IProxyClient> { return nullptr; };
}

asio::ip::tcp::endpoint
ProxyFactory::create_socks5_endpoint(const ProxyConfig &config) {
  boost::system::error_code ec;
  auto socks_ip = asio::ip::make_address(config.socks5_host, ec);

  if (ec) {
    throw std::runtime_error("Invalid SOCKS5 IP address in configuration: " +
                             ec.message());
  }

  return asio::ip::tcp::endpoint(socks_ip, config.socks5_port);
}

} // namespace stack::proxy
