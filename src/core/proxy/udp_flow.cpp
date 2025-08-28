// udp_flow.cpp
#include "udp_flow.hpp"
#include <boost/asio/ip/udp.hpp>

namespace proxy {

udp_flow::udp_flow(boost::asio::io_context &io,
                   std::unique_ptr<clients::iproxy_client> client)
    : io_(io), client_(std::move(client)) {}

void udp_flow::async_send(const std::string &dest_host, uint16_t dest_port,
                          const std::vector<uint8_t> &data,
                          std::function<void(bool)> handler) {
  // Example: open UDP socket to proxy, use client_->connect_via_proxy if needed
  // ...send UDP packet via proxy logic here...
  // handler(true/false);
}

} // namespace proxy
