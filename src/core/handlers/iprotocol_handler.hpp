#pragma once

#include <tins/tins.h>

// An abstract base class (interface) that defines the contract for all
// Layer 4 protocol handlers (TCP, UDP, ICMP, etc.).
class IProtocolHandler
{
public:
  virtual ~IProtocolHandler() = default;

  // Handles an incoming IPv4 packet.
  // The dispatcher calls this when it receives an IPv4 packet whose protocol
  // number matches the one this handler is registered for.
  // @param packet The fully parsed Tins::IP packet.
  virtual void handle_ipv4(std::unique_ptr<Tins::IP> packet) = 0;

  // Handles an incoming IPv6 packet.
  // The dispatcher calls this when it receives an IPv6 packet whose next_header
  // field matches the one this handler is registered for.
  // @param packet The fully parsed Tins::IPv6 packet.
  virtual void handle_ipv6(std::unique_ptr<Tins::IPv6> packet) = 0;
};