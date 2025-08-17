#pragma once

#include <map>

#include "definitions.hpp"
#include "handlers/iprotocol_handler.hpp"


class Dispatcher
{
public:
    Dispatcher(
        const asio::any_io_executor& executor,
        IPPacketChannel& incoming_channel,
        IPPacketChannel& outgoing_channel
    );

    // Destructor brings the interface down.
    ~Dispatcher();

    // Deleted copy/move operations to prevent resource mismanagement.
    Dispatcher(const Dispatcher&) = delete;
    Dispatcher& operator=(const Dispatcher&) = delete;
    Dispatcher(Dispatcher&&) = delete;
    Dispatcher& operator=(Dispatcher&&) = delete;


    // public func
    asio::awaitable<void> run();

private:
    asio::any_io_executor executor_;
    IPPacketChannel& incoming_channel_;
    IPPacketChannel& outgoing_channel_;

    using HandlersMap = std::map<uint8_t, std::unique_ptr<IProtocolHandler>>;
    HandlersMap handlers_;
};