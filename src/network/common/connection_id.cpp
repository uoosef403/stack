#include "connection_id.hpp"

namespace asio = boost::asio;

namespace stack::network {

// Constructor from individual parts
ConnectionId::ConnectionId(asio::ip::address s_ip, asio::ip::address d_ip,
						   uint16_t s_port, uint16_t d_port)
	: src_ip_(std::move(s_ip)), dst_ip_(std::move(d_ip)), src_port_(s_port),
	  dst_port_(d_port){};

// equality operator overloading for std::map
bool ConnectionId::operator==(const ConnectionId &other) const noexcept {
	return std::tie(src_ip_, dst_ip_, src_port_, dst_port_) ==
		   std::tie(other.src_ip_, other.dst_ip_, other.src_port_,
					other.dst_port_);
}

// less-than operator overloading for std::map
bool ConnectionId::operator<(const ConnectionId &other) const noexcept {
	return std::tie(src_ip_, dst_ip_, src_port_, dst_port_) <
		   std::tie(other.src_ip_, other.dst_ip_, other.src_port_,
					other.dst_port_);
}

} // namespace stack::network
