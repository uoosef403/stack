#include <gtest/gtest.h>
#include "network/common/connection_id.hpp"
#include <vector>
#include <tuple>
#include <string>

class ConnectionIdTest : public ::testing::Test {
protected:
};

static void RunEqualityCase(const std::string& src,
                            const std::string& dst,
                            const std::string& alt_src) {
    stack::network::ConnectionId id1(
        asio::ip::make_address(src),
        asio::ip::make_address(dst),
        1024,
        80
    );

    stack::network::ConnectionId id2(
        asio::ip::make_address(src),
        asio::ip::make_address(dst),
        1024,
        80
    );

    stack::network::ConnectionId id3(
        asio::ip::make_address(alt_src),
        asio::ip::make_address(dst),
        1024,
        80
    );

    EXPECT_EQ(id1, id2);
    EXPECT_NE(id1, id3);
}

static void RunLessThanCases(const std::string& src_a,
                             const std::string& src_b,
                             const std::string& dst) {
    // source IP difference
    stack::network::ConnectionId id1(asio::ip::make_address(src_a),
                                     asio::ip::make_address(dst),
                                     1000,
                                     80);
    stack::network::ConnectionId id2(asio::ip::make_address(src_b),
                                     asio::ip::make_address(dst),
                                     1000,
                                     80);
    EXPECT_LT(id1, id2);
    EXPECT_FALSE(id2 < id1);

    // source port difference (same addresses)
    stack::network::ConnectionId id3(asio::ip::make_address(src_a),
                                     asio::ip::make_address(dst),
                                     1000,
                                     80);
    stack::network::ConnectionId id4(asio::ip::make_address(src_a),
                                     asio::ip::make_address(dst),
                                     1001,
                                     80);
    EXPECT_LT(id3, id4);
    EXPECT_FALSE(id4 < id3);
}

TEST_F(ConnectionIdTest, EqualityOperator_IPv4_and_IPv6) {
    RunEqualityCase("1.1.1.1", "2.2.2.2", "9.9.9.9");
    RunEqualityCase("2001:db8::1", "2001:db8::2", "2001:db8::9");
}

TEST_F(ConnectionIdTest, LessThanOperator_IPv4_and_IPv6) {
    RunLessThanCases("1.1.1.1", "1.1.1.2", "8.8.8.8");
    RunLessThanCases("2001:db8::1", "2001:db8::2", "2001:4860:4860::8888");
}
