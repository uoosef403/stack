#pragma once

#include <type_traits>
#include <boost/asio.hpp>
#include <tins/tins.h>

namespace asio = boost::asio;

namespace stack::network {
class ConnectionId {
  public:
	// Constructor from individual parts
	ConnectionId(asio::ip::address s_ip, asio::ip::address d_ip,
				 uint16_t s_port, uint16_t d_port);

	// Constructor from a Tins packet
	 template<typename T>
      explicit ConnectionId(const T &packet)
        requires(std::is_same_v<T, Tins::IP> || std::is_same_v<T, Tins::IPv6>);

	// Public Getters
	[[nodiscard]] const asio::ip::address &src_ip() const { return src_ip_; }
	[[nodiscard]] const asio::ip::address &dst_ip() const { return dst_ip_; }
	[[nodiscard]] uint16_t src_port() const { return src_port_; }
	[[nodiscard]] uint16_t dst_port() const { return dst_port_; }

	// Comparison operators
	bool operator==(const ConnectionId &other) const;
	bool operator<(const ConnectionId &other) const;

  private:
	asio::ip::address src_ip_;
	asio::ip::address dst_ip_;
	uint16_t src_port_;
	uint16_t dst_port_;
};
} // namespace stack::network
