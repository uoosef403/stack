#pragma once
#include <utility>

#include "tcp_state.hpp"

namespace stack::tcp {
    struct four_tuple {
        asio::ip::address src_ip;
        asio::ip::address dst_ip;
        uint16_t src_port;
        uint16_t dst_port;

        // Constructor for easy initialization
        four_tuple(asio::ip::address s_ip, asio::ip::address d_ip, uint16_t s_port, uint16_t d_port)
            : src_ip(std::move(s_ip)), dst_ip(std::move(d_ip)), src_port(s_port), dst_port(d_port) {}

        // Comparison operator for use in maps
        bool operator<(const four_tuple& other) const {
            return std::tie(src_ip, dst_ip, src_port, dst_port) <
                   std::tie(other.src_ip, other.dst_ip, other.src_port, other.dst_port);
        }
    };

    class Pcb {
    public:
        explicit Pcb(four_tuple tuple)
            : id_(std::move(tuple)), state_(TcpState::CLOSED) {}

        four_tuple id_;
        TcpState state_;

        // Sequence and Acknowledgment numbers.
        // We need to track what we've sent and what we expect to receive.
        struct {
            uint32_t next = 0; // SND.NXT: Next sequence number to send.
            uint32_t unacknowledged = 0; // SND.UNA: Oldest unacknowledged sequence number.
            uint32_t window = 0; // SND.WND: The receive window of our peer.
        } send_vars_;

        struct {
            uint32_t next = 0; // RCV.NXT: Next sequence number expected.
            uint32_t window = 65535; // RCV.WND: Our receive window size.
        } recv_vars_;
    };
}