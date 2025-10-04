#include "config.hpp"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <fstream>
#include <iostream>


namespace po = boost::program_options;
namespace fs = boost::filesystem;

Config &Config::getInstance() {
  static Config instance;
  return instance;
}

bool Config::loadFromFile(const std::string &filename) {
  if (!fs::exists(filename)) {
    return false;
  }

  try {
    po::options_description config("Configuration");
    config.add_options()
        // Network configuration
        ("network.interface_name",
         po::value<std::string>(&network_config.interface_name)
             ->default_value("tun0"),
         "TUN interface name")(
            "network.interface_ip",
            po::value<std::string>(&network_config.interface_ip)
                ->default_value("10.0.0.1"),
            "TUN interface IP address")(
            "network.interface_netmask",
            po::value<std::string>(&network_config.interface_netmask)
                ->default_value("255.255.255.0"),
            "TUN interface netmask")(
            "network.mtu",
            po::value<uint16_t>(&network_config.mtu)->default_value(1500),
            "Maximum transmission unit")

        // Proxy configuration
        ("proxy.enable_socks5",
         po::value<bool>(&proxy_config.enable_socks5)->default_value(false),
         "Enable SOCKS5 proxy")(
            "proxy.socks5_host",
            po::value<std::string>(&proxy_config.socks5_host)
                ->default_value("127.0.0.1"),
            "SOCKS5 proxy host")(
            "proxy.socks5_port",
            po::value<uint16_t>(&proxy_config.socks5_port)->default_value(1080),
            "SOCKS5 proxy port")(
            "proxy.socks5_username",
            po::value<std::string>(&proxy_config.socks5_username),
            "SOCKS5 proxy username")(
            "proxy.socks5_password",
            po::value<std::string>(&proxy_config.socks5_password),
            "SOCKS5 proxy password")

        // Logging configuration
        ("logging.log_level",
         po::value<std::string>(&logging_config.log_level)
             ->default_value("info"),
         "Log level (trace, debug, info, warn, error, critical)")(
            "logging.log_file",
            po::value<std::string>(&logging_config.log_file),
            "Log file path (empty for no file logging)")(
            "logging.console_output",
            po::value<bool>(&logging_config.console_output)
                ->default_value(true),
            "Enable console output")

        // TCP configuration
        ("tcp.connection_timeout_ms",
         po::value<uint32_t>(&tcp_config.connection_timeout_ms)
             ->default_value(30000),
         "TCP connection timeout in milliseconds")(
            "tcp.keep_alive_interval_ms",
            po::value<uint32_t>(&tcp_config.keep_alive_interval_ms)
                ->default_value(60000),
            "TCP keep-alive interval in milliseconds")(
            "tcp.max_connections",
            po::value<uint16_t>(&tcp_config.max_connections)
                ->default_value(1000),
            "Maximum TCP connections")(
            "tcp.buffer_size",
            po::value<uint32_t>(&tcp_config.buffer_size)->default_value(65536),
            "TCP buffer size in bytes")

        // UDP configuration
        ("udp.timeout_ms",
         po::value<uint32_t>(&udp_config.timeout_ms)->default_value(5000),
         "UDP timeout in milliseconds")(
            "udp.max_connections",
            po::value<uint16_t>(&udp_config.max_connections)
                ->default_value(500),
            "Maximum UDP connections")

        // ICMP configuration
        ("icmp.enable",
         po::value<bool>(&icmp_config.enable)->default_value(true),
         "Enable ICMP processing")(
            "icmp.timeout_ms",
            po::value<uint32_t>(&icmp_config.timeout_ms)->default_value(5000),
            "ICMP timeout in milliseconds");

    po::variables_map vm;
    std::ifstream config_file(filename);
    po::store(po::parse_config_file(config_file, config), vm);
    po::notify(vm);

    return true;
  } catch (const std::exception &e) {
    std::cerr << "Error parsing config file: " << e.what() << std::endl;
    return false;
  }
}

bool Config::parseCommandLine(int argc, char *argv[]) {
  try {
    po::options_description desc("Network Stack Options");
    desc.add_options()("help,h", "Show this help message")(
        "config,c", po::value<std::string>(), "Configuration file path")(
        "interface,i", po::value<std::string>(&network_config.interface_name),
        "TUN interface name")(
        "ip", po::value<std::string>(&network_config.interface_ip),
        "TUN interface IP address")(
        "netmask", po::value<std::string>(&network_config.interface_netmask),
        "TUN interface netmask")("mtu",
                                 po::value<uint16_t>(&network_config.mtu),
                                 "Maximum transmission unit")(
        "socks5-enable", po::value<bool>(&proxy_config.enable_socks5),
        "Enable SOCKS5 proxy")(
        "socks5-host", po::value<std::string>(&proxy_config.socks5_host),
        "SOCKS5 proxy host")("socks5-port",
                             po::value<uint16_t>(&proxy_config.socks5_port),
                             "SOCKS5 proxy port")(
        "socks5-user", po::value<std::string>(&proxy_config.socks5_username),
        "SOCKS5 proxy username")(
        "socks5-pass", po::value<std::string>(&proxy_config.socks5_password),
        "SOCKS5 proxy password")(
        "log-level", po::value<std::string>(&logging_config.log_level),
        "Log level (trace, debug, info, warn, error, critical)")(
        "log-file", po::value<std::string>(&logging_config.log_file),
        "Log file path")("no-console", "Disable console output")(
        "tcp-timeout", po::value<uint32_t>(&tcp_config.connection_timeout_ms),
        "TCP connection timeout in milliseconds")(
        "tcp-keepalive",
        po::value<uint32_t>(&tcp_config.keep_alive_interval_ms),
        "TCP keep-alive interval in milliseconds")(
        "tcp-max-conn", po::value<uint16_t>(&tcp_config.max_connections),
        "Maximum TCP connections")("tcp-buffer",
                                   po::value<uint32_t>(&tcp_config.buffer_size),
                                   "TCP buffer size in bytes")(
        "udp-timeout", po::value<uint32_t>(&udp_config.timeout_ms),
        "UDP timeout in milliseconds")(
        "udp-max-conn", po::value<uint16_t>(&udp_config.max_connections),
        "Maximum UDP connections")("no-icmp", "Disable ICMP processing")(
        "icmp-timeout", po::value<uint32_t>(&icmp_config.timeout_ms),
        "ICMP timeout in milliseconds");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);

    if (vm.count("help")) {
      std::cout << desc << std::endl;
      return false;
    }

    if (vm.count("config")) {
      std::string config_file = vm["config"].as<std::string>();
      if (!loadFromFile(config_file)) {
        std::cerr << "Failed to load configuration file: " << config_file
                  << std::endl;
        return false;
      }
    }

    if (vm.count("no-console")) {
      logging_config.console_output = false;
    }

    if (vm.count("no-icmp")) {
      icmp_config.enable = false;
    }

    po::notify(vm);
    return true;
  } catch (const std::exception &e) {
    std::cerr << "Error parsing command line: " << e.what() << std::endl;
    return false;
  }
}

void Config::printHelp() const {
  std::cout << R"(Network Stack - High Performance User-Space Network Stack

Usage: stack [options]

Configuration file format (INI style):
[network]
interface_name=tun0
interface_ip=10.0.0.1
interface_netmask=255.255.255.0
mtu=1500

[proxy]
enable_socks5=false
socks5_host=127.0.0.1
socks5_port=1080

[logging]
log_level=info
console_output=true

[tcp]
connection_timeout_ms=30000
keep_alive_interval_ms=60000
max_connections=1000
buffer_size=65536

[udp]
timeout_ms=5000
max_connections=500

[icmp]
enable=true
timeout_ms=5000

Examples:
  stack                           # Use default configuration
  stack -c config.ini             # Load configuration from file
  stack --ip 192.168.1.1 --socks5-enable true --socks5-host proxy.example.com
)";
}
