#include "config.hpp"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <iostream>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

Config &Config::get_instance() {
  static Config instance;
  return instance;
}

void Config::load_defaults() {
  // Network defaults
  network_config_ = NetworkConfig{};

  // Proxy defaults
  proxy_config_ = ProxyConfig{};

  // Logging defaults
  logging_config_ = LoggingConfig{};

  // TCP defaults
  tcp_config_ = TcpConfig{};

  // UDP defaults
  udp_config_ = UdpConfig{};

  // ICMP defaults
  icmp_config_ = IcmpConfig{};
}

bool Config::load_from_file(const std::string &filename) {
  if (!fs::exists(filename)) {
    std::cerr << "Configuration file not found: " << filename << '\n';
    return false;
  }

  try {
    // Load defaults first
    load_defaults();

    // Parse TOML file
    auto config = toml::parse_file(filename);

    // Load each section
    load_network_config(config);
    load_proxy_config(config);
    load_logging_config(config);
    load_tcp_config(config);
    load_udp_config(config);
    load_icmp_config(config);

    return true;
  } catch (const toml::parse_error &e) {
    std::cerr << "TOML parsing error in " << filename << ":\n" << e << '\n';
    return false;
  } catch (const std::exception &e) {
    std::cerr << "Error loading configuration file: " << e.what() << '\n';
    return false;
  }
}

void Config::load_network_config(const toml::table &config) {
  if (const auto *network = config["network"].as_table()) {
    if (auto interface_name =
            (*network)["interface_name"].value<std::string>()) {
      network_config_.interface_name = *interface_name;
    }
    if (auto interface_ip = (*network)["interface_ip"].value<std::string>()) {
      network_config_.interface_ip = *interface_ip;
    }
    if (auto interface_netmask =
            (*network)["interface_netmask"].value<std::string>()) {
      network_config_.interface_netmask = *interface_netmask;
    }
    if (auto ipv6_address = (*network)["ipv6_address"].value<std::string>()) {
      network_config_.ipv6_address = *ipv6_address;
    }
    if (auto ipv6_prefix = (*network)["ipv6_prefix"].value<uint8_t>()) {
      network_config_.ipv6_prefix = *ipv6_prefix;
    }
    if (auto mtu = (*network)["mtu"].value<uint16_t>()) {
      network_config_.mtu = *mtu;
    }
  }
}

void Config::load_proxy_config(const toml::table &config) {
  if (const auto *proxy = config["proxy"].as_table()) {
    if (auto enable_socks5 = (*proxy)["enable_socks5"].value<bool>()) {
      proxy_config_.enable_socks5 = *enable_socks5;
    }
    if (auto socks5_host = (*proxy)["socks5_host"].value<std::string>()) {
      proxy_config_.socks5_host = *socks5_host;
    }
    if (auto socks5_port = (*proxy)["socks5_port"].value<uint16_t>()) {
      proxy_config_.socks5_port = *socks5_port;
    }
    if (auto socks5_username =
            (*proxy)["socks5_username"].value<std::string>()) {
      proxy_config_.socks5_username = *socks5_username;
    }
    if (auto socks5_password =
            (*proxy)["socks5_password"].value<std::string>()) {
      proxy_config_.socks5_password = *socks5_password;
    }
  }
}

void Config::load_logging_config(const toml::table &config) {
  if (const auto *logging = config["logging"].as_table()) {
    if (auto log_level = (*logging)["log_level"].value<std::string>()) {
      logging_config_.log_level = *log_level;
    }
    if (auto log_file = (*logging)["log_file"].value<std::string>()) {
      logging_config_.log_file = *log_file;
    }
    if (auto console_output = (*logging)["console_output"].value<bool>()) {
      logging_config_.console_output = *console_output;
    }
  }
}

void Config::load_tcp_config(const toml::table &config) {
  if (const auto *tcp = config["tcp"].as_table()) {
    if (auto connection_timeout_ms =
            (*tcp)["connection_timeout_ms"].value<uint32_t>()) {
      tcp_config_.connection_timeout_ms = *connection_timeout_ms;
    }
    if (auto keep_alive_interval_ms =
            (*tcp)["keep_alive_interval_ms"].value<uint32_t>()) {
      tcp_config_.keep_alive_interval_ms = *keep_alive_interval_ms;
    }
    if (auto max_connections = (*tcp)["max_connections"].value<uint16_t>()) {
      tcp_config_.max_connections = *max_connections;
    }
    if (auto buffer_size = (*tcp)["buffer_size"].value<uint32_t>()) {
      tcp_config_.buffer_size = *buffer_size;
    }
  }
}

void Config::load_udp_config(const toml::table &config) {
  if (const auto *udp = config["udp"].as_table()) {
    if (auto timeout_ms = (*udp)["timeout_ms"].value<uint32_t>()) {
      udp_config_.timeout_ms = *timeout_ms;
    }
    if (auto max_connections = (*udp)["max_connections"].value<uint16_t>()) {
      udp_config_.max_connections = *max_connections;
    }
  }
}

void Config::load_icmp_config(const toml::table &config) {
  if (const auto *icmp = config["icmp"].as_table()) {
    if (auto enable = (*icmp)["enable"].value<bool>()) {
      icmp_config_.enable = *enable;
    }
    if (auto timeout_ms = (*icmp)["timeout_ms"].value<uint32_t>()) {
      icmp_config_.timeout_ms = *timeout_ms;
    }
  }
}

bool Config::parse_command_line(int argc, char *argv[]) {
  try {
    po::options_description desc("Network Stack Options");
    desc.add_options()("help,h", "Show this help message")(
        "config,c", po::value<std::string>(),
        "Configuration file path (TOML format)")(
        "interface,i", po::value<std::string>(&network_config_.interface_name),
        "TUN interface name")(
        "ip", po::value<std::string>(&network_config_.interface_ip),
        "TUN interface IP address")(
        "netmask", po::value<std::string>(&network_config_.interface_netmask),
        "TUN interface netmask")("mtu",
                                 po::value<uint16_t>(&network_config_.mtu),
                                 "Maximum transmission unit")(
        "socks5-enable", po::value<bool>(&proxy_config_.enable_socks5),
        "Enable SOCKS5 proxy")(
        "socks5-host", po::value<std::string>(&proxy_config_.socks5_host),
        "SOCKS5 proxy host")("socks5-port",
                             po::value<uint16_t>(&proxy_config_.socks5_port),
                             "SOCKS5 proxy port")(
        "socks5-user", po::value<std::string>(&proxy_config_.socks5_username),
        "SOCKS5 proxy username")(
        "socks5-pass", po::value<std::string>(&proxy_config_.socks5_password),
        "SOCKS5 proxy password")(
        "log-level", po::value<std::string>(&logging_config_.log_level),
        "Log level (trace, debug, info, warn, error, critical)")(
        "log-file", po::value<std::string>(&logging_config_.log_file),
        "Log file path")("no-console", "Disable console output")(
        "tcp-timeout", po::value<uint32_t>(&tcp_config_.connection_timeout_ms),
        "TCP connection timeout in milliseconds")(
        "tcp-keepalive",
        po::value<uint32_t>(&tcp_config_.keep_alive_interval_ms),
        "TCP keep-alive interval in milliseconds")(
        "tcp-max-conn", po::value<uint16_t>(&tcp_config_.max_connections),
        "Maximum TCP connections")(
        "tcp-buffer", po::value<uint32_t>(&tcp_config_.buffer_size),
        "TCP buffer size in bytes")(
        "udp-timeout", po::value<uint32_t>(&udp_config_.timeout_ms),
        "UDP timeout in milliseconds")(
        "udp-max-conn", po::value<uint16_t>(&udp_config_.max_connections),
        "Maximum UDP connections")("no-icmp", "Disable ICMP processing")(
        "icmp-timeout", po::value<uint32_t>(&icmp_config_.timeout_ms),
        "ICMP timeout in milliseconds");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);

    if (vm.count("help")) { // NOLINT
      std::cout << desc << '\n';
      return false;
    }

    if (vm.count("config")) { // NOLINT
      std::string config_file = vm["config"].as<std::string>();
      if (!load_from_file(config_file)) {
        std::cerr << "Failed to load configuration file: " << config_file
                  << '\n';
        return false;
      }
    } else {
      // Load defaults if no config file specified
      load_defaults();
    }

    if (vm.count("no-console")) { // NOLINT
      logging_config_.console_output = false;
    }

    if (vm.count("no-icmp")) { // NOLINT
      icmp_config_.enable = false;
    }

    po::notify(vm);
    return true;
  } catch (const std::exception &e) {
    std::cerr << "Error parsing command line: " << e.what() << '\n';
    return false;
  }
}

void Config::print_help() {
  std::cout << R"(Network Stack - High Performance User-Space Network Stack

Usage: stack [options]

Configuration file format (TOML):
[network]
interface_name = "tun0"
interface_ip = "10.0.0.1"
interface_netmask = "255.255.255.0"
mtu = 1500

[proxy]
enable_socks5 = false
socks5_host = "127.0.0.1"
socks5_port = 1080

[logging]
log_level = "info"
console_output = true

[tcp]
connection_timeout_ms = 30000
keep_alive_interval_ms = 60000
max_connections = 1000
buffer_size = 65536

[udp]
timeout_ms = 5000
max_connections = 500

[icmp]
enable = true
timeout_ms = 5000

Examples:
  stack                           # Use default configuration
  stack -c config.toml            # Load configuration from TOML file
  stack --ip 192.168.1.1 --socks5-enable true --socks5-host proxy.example.com
)";
}
