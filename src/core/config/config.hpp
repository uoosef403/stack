#ifndef CORE_CONFIG_CONFIG_HPP
#define CORE_CONFIG_CONFIG_HPP

#include <cstdint>
#include <string>
#include <toml++/toml.hpp>

struct NetworkConfig {
  std::string interface_name = "tun0";
  std::string interface_ip = "10.0.0.1";
  std::string interface_netmask = "255.255.255.0";
  std::string ipv6_address = "fd69::69";
  uint8_t ipv6_prefix = 64;
  uint16_t mtu = 1500;
};

struct ProxyConfig {
  bool enable_socks5 = false;
  std::string socks5_host = "127.0.0.1";
  uint16_t socks5_port = 1080;
  std::string socks5_username;
  std::string socks5_password;
};

struct LoggingConfig {
  std::string log_level = "info";
  std::string log_file;
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
  static Config &get_instance();

  bool load_from_file(const std::string &filename);
  bool parse_command_line(int argc, char *argv[]);
  static void print_help();

  // Getters
  const NetworkConfig &get_network_config() const { return network_config_; }
  const ProxyConfig &get_proxy_config() const { return proxy_config_; }
  const LoggingConfig &get_logging_config() const { return logging_config_; }
  const TcpConfig &get_tcp_config() const { return tcp_config_; }
  const UdpConfig &get_udp_config() const { return udp_config_; }
  const IcmpConfig &get_icmp_config() const { return icmp_config_; }

  Config(const Config &) = delete;
  Config &operator=(const Config &) = delete;
  Config(Config &&) = delete;
  Config &operator=(Config &&) = delete;
  ~Config() = default;

private:
  Config() = default;

  void load_defaults();
  void load_network_config(const toml::table &config);
  void load_proxy_config(const toml::table &config);
  void load_logging_config(const toml::table &config);
  void load_tcp_config(const toml::table &config);
  void load_udp_config(const toml::table &config);
  void load_icmp_config(const toml::table &config);

  NetworkConfig network_config_;
  ProxyConfig proxy_config_;
  LoggingConfig logging_config_;
  TcpConfig tcp_config_;
  UdpConfig udp_config_;
  IcmpConfig icmp_config_;
};

#endif // CORE_CONFIG_CONFIG_HPP
