// client.hpp (SOCKS5)
#pragma once
#include "../iproxy_client.hpp"
#include <cstdint>
#include <string>

namespace proxy {
namespace clients {
namespace socks5 {

class Client : public iproxy_client {
public:
  // Implements SOCKS5 handshake and connect
  bool connect_via_proxy(int sock, const std::string &dest_host,
                         uint16_t dest_port) override;
};

} // namespace socks5
} // namespace clients
} // namespace proxy
