// udp_flow.hpp
#pragma once
#include <memory>
#include <string>
#include <cstdint>
#include <boost/asio.hpp>
#include "clients/iproxy_client.hpp"

namespace proxy {

class udp_flow {
public:
	udp_flow(boost::asio::io_context& io, std::unique_ptr<clients::iproxy_client> client);
	// Send a UDP packet to dest_host:dest_port via the proxy
	void async_send(const std::string& dest_host, uint16_t dest_port, const std::vector<uint8_t>& data, std::function<void(bool)> handler);

private:
	boost::asio::io_context& io_;
	std::unique_ptr<clients::iproxy_client> client_;
};

} // namespace proxy
