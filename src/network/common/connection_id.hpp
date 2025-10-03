#pragma once

#include <boost/asio.hpp>
#include <tins/tins.h>

namespace stack::network {

// Helper functions to convert Tins addresses to Asio addresses
inline boost::asio::ip::address to_asio(const Tins::IPv4Address &a) {
	return boost::asio::ip::make_address_v4(a.to_string());
}

inline boost::asio::ip::address to_asio(const Tins::IPv6Address &a) {
	return boost::asio::ip::make_address_v6(a.to_string());
}

class ConnectionId {
	public:
	// Constructor from individual parts
	ConnectionId(boost::asio::ip::address s_ip, boost::asio::ip::address d_ip,
				 uint16_t s_port, uint16_t d_port);

	// Constructor from a Tins packet
	template <typename T>
	explicit ConnectionId(const T &packet)
		requires(std::is_same_v<T, Tins::IP> || std::is_same_v<T, Tins::IPv6>) {
		const auto *tcp = packet.template find_pdu<Tins::TCP>();
		if (!tcp) {
			throw std::runtime_error(
				"four_tuple created from non-TCP packet");
		}

		src_ip_ = to_asio(packet.src_addr());
		dst_ip_ = to_asio(packet.dst_addr());
		src_port_ = tcp->sport();
		dst_port_ = tcp->dport();
	}

	// Public Getters
	[[nodiscard]] const boost::asio::ip::address &src_ip() const noexcept { return src_ip_; }
	[[nodiscard]] const boost::asio::ip::address &dst_ip() const noexcept { return dst_ip_; }
	[[nodiscard]] uint16_t src_port() const noexcept { return src_port_; }
	[[nodiscard]] uint16_t dst_port() const noexcept { return dst_port_; }
	[[nodiscard]] bool is_ipv4() const { return src_ip_.is_v4(); }
	[[nodiscard]] bool is_ipv6() const { return src_ip_.is_v6(); }
	[[nodiscard]] std::string to_string() const {
		return src_ip_.to_string() + ":" + std::to_string(src_port_) + " -> " +
				 dst_ip_.to_string() + ":" + std::to_string(dst_port_);
	}
	// Comparison operators
	[[nodiscard]] bool operator==(const ConnectionId &other) const noexcept;
	[[nodiscard]] bool operator<(const ConnectionId &other) const noexcept;

	private:
	boost::asio::ip::address src_ip_;
	boost::asio::ip::address dst_ip_;
	uint16_t src_port_;
	uint16_t dst_port_;
};
} // namespace stack::network

namespace std {
template <> struct hash<stack::network::ConnectionId> {
	std::size_t
	operator()(const stack::network::ConnectionId &id) const noexcept {
		std::size_t h1 = std::hash<std::string>{}(id.src_ip().to_string());
		std::size_t h2 = std::hash<std::string>{}(id.dst_ip().to_string());
		std::size_t h3 = std::hash<uint16_t>{}(id.src_port());
		std::size_t h4 = std::hash<uint16_t>{}(id.dst_port());

		return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
	}
};
} // namespace std
