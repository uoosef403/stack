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
            : src_ip(std::move(s_ip)), dst_ip(std::move(d_ip)), src_port(s_port), dst_port(d_port) {
        }

        static asio::ip::address to_asio(const Tins::IPv4Address &a) {
            return asio::ip::make_address_v4(a.to_string());
        }

        static asio::ip::address to_asio(const Tins::IPv6Address &a) {
            return asio::ip::make_address_v6(a.to_string());
        }

        // Returns true if this tuple uses IPv6 addresses
        bool is_ipv6() const {
            return src_ip.is_v6() && dst_ip.is_v6();
        }

        template<typename T>
        explicit four_tuple(const T &packet)
            requires (std::is_same_v<T, Tins::IP> || std::is_same_v<T, Tins::IPv6>) {
            const auto *tcp = packet.template find_pdu<Tins::TCP>();
            if (!tcp) {
                throw std::runtime_error("four_tuple created from non-TCP packet");
            }

            src_ip = to_asio(packet.src_addr());
            dst_ip = to_asio(packet.dst_addr());
            src_port = tcp->sport();
            dst_port = tcp->dport();
        }

        // Comparison operator for use in maps
        bool operator<(const four_tuple &other) const {
            return std::tie(src_ip, dst_ip, src_port, dst_port) <
                   std::tie(other.src_ip, other.dst_ip, other.src_port, other.dst_port);
        }
    };

    class Pcb {
    public:
        explicit Pcb(four_tuple tuple)
            : id(std::move(tuple)), state(TcpState::CLOSED) {
        }

        four_tuple id;
        TcpState state;

        // Sequence and Acknowledgment numbers.
        // We need to track what we've sent and what we expect to receive.
        struct {
            uint32_t next = 0; // SND.NXT: Next sequence number to send.
            uint32_t unacknowledged = 0; // SND.UNA: Oldest unacknowledged sequence number.
            uint16_t window = 0; // SND.WND: The receive window of our peer.
        } send_vars;

        struct {
            uint32_t next = 0; // RCV.NXT: Next sequence number expected.
            uint16_t window = 65535; // RCV.WND: Our receive window size.
        } recv_vars;
    };
}
