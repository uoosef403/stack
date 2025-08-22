#pragma once
#include <string_view>

#include "definitions.hpp"

enum class TcpState {
    CLOSED = 0,        // The connection is closed.
    LISTEN = 1,        // The server is waiting for a connection request.
    SYN_SENT = 2,      // The client has sent a SYN and is waiting for a SYN-ACK.
    SYN_RECEIVED = 3,  // The server has received a SYN and sent a SYN-ACK.
    ESTABLISHED = 4,   // The connection is established and data can be sent.
    FIN_WAIT_1 = 5,   // The connection is closing; the sender has sent a FIN.
    FIN_WAIT_2 = 6,   // The sender has received an ACK for its FIN.
    CLOSE_WAIT = 7,   // The receiver has received a FIN and is waiting to close.
    CLOSING = 8,      // Both sides have sent FINs and are closing the connection.
    LAST_ACK = 9,     // The receiver has sent a FIN and is waiting for an ACK.
    TIME_WAIT = 10     // The connection is in TIME_WAIT state before being closed.
};

inline std::string_view tcp_state_to_string(const TcpState state) {
    switch (state) {
        case TcpState::CLOSED: return "CLOSED";
        case TcpState::LISTEN: return "LISTEN";
        case TcpState::SYN_SENT: return "SYN_SENT";
        case TcpState::SYN_RECEIVED: return "SYN_RECEIVED";
        case TcpState::ESTABLISHED: return "ESTABLISHED";
        case TcpState::FIN_WAIT_1: return "FIN_WAIT_1";
        case TcpState::FIN_WAIT_2: return "FIN_WAIT_2";
        case TcpState::CLOSE_WAIT: return "CLOSE_WAIT";
        case TcpState::CLOSING: return "CLOSING";
        case TcpState::LAST_ACK: return "LAST_ACK";
        case TcpState::TIME_WAIT: return "TIME_WAIT";
        default: {
            // This should never happen, but if it does we call debugger immedietly
            debug_break();
            return "UNKNOWN";
        }
    }
}