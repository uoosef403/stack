#pragma once

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>

namespace  asio = boost::asio;

using IPPacketChannel = asio::experimental::channel<void(boost::system::error_code, std::vector<uint8_t>)>;