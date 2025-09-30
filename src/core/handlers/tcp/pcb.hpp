#pragma once

#include <tins/tins.h>
#include "network/common/connection_id.hpp"
#include "tcp_state.hpp"

namespace stack::tcp {
    class Pcb {
    public:
        explicit Pcb(asio::any_io_executor executor, stack::network::ConnectionId id)
            : executor_(std::move(executor)),
              id(std::move(id)),
              state(TcpState::CLOSED),
              ctp_buf(executor_, 128),
              ptc_buf(executor_, 128) {
        }
        asio::any_io_executor executor_;

        stack::network::ConnectionId id;
        TcpState state;

        // Buffer for data going from the TcpFlow DOWN to the client app.
        IPPacketChannel ctp_buf;
        // Buffer for data going from the proxy UP to the TcpFlow.
        IPPacketChannel ptc_buf;

        // Sequence and Acknowledgment numbers.
        // We need to track what we've sent and what we expect to receive.
        struct {
            uint32_t next = 0; // SND.NXT: Next sequence number to send.
            uint32_t unacknowledged =
                    0; // SND.UNA: Oldest unacknowledged sequence number.
            uint16_t window = 0; // SND.WND: The receive window of our peer.
        } send_vars;

        struct {
            uint32_t next = 0; // RCV.NXT: Next sequence number expected.
            uint16_t window = 65535; // RCV.WND: Our receive window size.
        } recv_vars;
    };
} // namespace stack::tcp
