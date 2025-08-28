// client.cpp (SOCKS5)
#include "client.hpp"
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace proxy {
namespace clients {
namespace socks5 {

namespace {
constexpr uint8_t SOCKS_VERSION = 0x05;
constexpr uint8_t NO_AUTH = 0x00;
constexpr uint8_t CONNECT = 0x01;
constexpr uint8_t IPV4 = 0x01;
constexpr uint8_t DOMAIN = 0x03;
constexpr uint8_t SUCCESS = 0x00;

bool send_all(int sock, const void* buf, size_t len) {
	size_t sent = 0;
	const uint8_t* ptr = static_cast<const uint8_t*>(buf);
	while (sent < len) {
		ssize_t n = send(sock, ptr + sent, len - sent, 0);
		if (n <= 0) return false;
		sent += n;
	}
	return true;
}

bool recv_all(int sock, void* buf, size_t len) {
	size_t recvd = 0;
	uint8_t* ptr = static_cast<uint8_t*>(buf);
	while (recvd < len) {
		ssize_t n = recv(sock, ptr + recvd, len - recvd, 0);
		if (n <= 0) return false;
		recvd += n;
	}
	return true;
}
} // anonymous namespace

bool Client::connect_via_proxy(int sock, const std::string& dest_host, uint16_t dest_port) {
	// 1. Send greeting (no authentication)
	uint8_t greeting[3] = {SOCKS_VERSION, 0x01, NO_AUTH};
	if (!send_all(sock, greeting, 3)) return false;

	// 2. Receive server choice
	uint8_t method_select[2];
	if (!recv_all(sock, method_select, 2)) return false;
	if (method_select[0] != SOCKS_VERSION || method_select[1] != NO_AUTH) return false;

	// 3. Send connect request
	std::vector<uint8_t> req;
	req.push_back(SOCKS_VERSION);
	req.push_back(CONNECT);
	req.push_back(0x00); // Reserved
	// Address type and address
	struct in_addr ipv4_addr;
	if (inet_pton(AF_INET, dest_host.c_str(), &ipv4_addr) == 1) {
		req.push_back(IPV4);
		uint8_t* addr_bytes = reinterpret_cast<uint8_t*>(&ipv4_addr);
		req.insert(req.end(), addr_bytes, addr_bytes + 4);
	} else {
		req.push_back(DOMAIN);
		req.push_back(dest_host.size());
		req.insert(req.end(), dest_host.begin(), dest_host.end());
	}
	req.push_back(dest_port >> 8);
	req.push_back(dest_port & 0xFF);
	if (!send_all(sock, req.data(), req.size())) return false;

	// 4. Receive reply
	uint8_t reply[4];
	if (!recv_all(sock, reply, 4)) return false;
	if (reply[0] != SOCKS_VERSION || reply[1] != SUCCESS) return false;
	uint8_t atyp = reply[3];
	size_t addr_len = 0;
	switch (atyp) {
		case IPV4: addr_len = 4; break;
		case DOMAIN: {
			uint8_t len;
			if (!recv_all(sock, &len, 1)) return false;
			addr_len = len;
			break;
		}
		default: return false;
	}
	std::vector<uint8_t> bind_addr(addr_len);
	if (!recv_all(sock, bind_addr.data(), addr_len)) return false;
	uint8_t port_bytes[2];
	if (!recv_all(sock, port_bytes, 2)) return false;
	// Now connected via proxy
	return true;
}

} // namespace socks5
} // namespace clients
} // namespace proxy
