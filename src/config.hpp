#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <cstdint>
#include <string>


struct NetworkConfig {
  std::string interface_name = "tun0";
  std::string interface_ip = "10.0.0.1";
  std::string interface_netmask = "255.255.255.0";
  uint16_t mtu = 1500;
};

struct ProxyConfig {
  bool enable_socks5 = false;
  std::string socks5_host = "127.0.0.1";
  uint16_t socks5_port = 1080;
  std::string socks5_username = "";
  std::string socks5_password = "";
};

struct LoggingConfig {
  std::string log_level = "info";
  std::string log_file = "";
  bool console_output = true;
};

struct TcpConfig {
  uint32_t connection_timeout_ms = 30000;
  uint32_t keep_alive_interval_ms = 60000;
  uint16_t max_connections = 1000;
  uint32_t buffer_size = 65536;
};

struct UdpConfig {
  uint32_t timeout_ms = 5000;
  uint16_t max_connections = 500;
};

struct IcmpConfig {
  bool enable = true;
  uint32_t timeout_ms = 5000;
};

class Config {
public:
  static Config &getInstance();

  bool loadFromFile(const std::string &filename);
  bool parseCommandLine(int argc, char *argv[]);
  void printHelp() const;

  // Getters
  const NetworkConfig &getNetworkConfig() const { return network_config; }
  const ProxyConfig &getProxyConfig() const { return proxy_config; }
  const LoggingConfig &getLoggingConfig() const { return logging_config; }
  const TcpConfig &getTcpConfig() const { return tcp_config; }
  const UdpConfig &getUdpConfig() const { return udp_config; }
  const IcmpConfig &getIcmpConfig() const { return icmp_config; }

private:
  Config() = default;
  Config(const Config &) = delete;
  Config &operator=(const Config &) = delete;

  NetworkConfig network_config;
  ProxyConfig proxy_config;
  LoggingConfig logging_config;
  TcpConfig tcp_config;
  UdpConfig udp_config;
  IcmpConfig icmp_config;
};

#endif // CONFIG_HPP
