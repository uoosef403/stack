#include <gtest/gtest.h>
#include "network/common/connection_id.hpp"

// Test fixture for the ConnectionId class
class ConnectionIdTest : public ::testing::Test {
protected:
};

TEST_F(ConnectionIdTest, EqualityOperator) {
    stack::network::ConnectionId id1(
        asio::ip::make_address("1.1.1.1"),
        asio::ip::make_address("2.2.2.2"),
        1024,
        80
    );

    stack::network::ConnectionId id2(
        asio::ip::make_address("1.1.1.1"),
        asio::ip::make_address("2.2.2.2"),
        1024,
        80
    );

    stack::network::ConnectionId id3(
        asio::ip::make_address("9.9.9.9"), // Different IP
        asio::ip::make_address("2.2.2.2"),
        1024,
        80
    );

    EXPECT_EQ(id1, id2);
    EXPECT_NE(id1, id3);
}

TEST_F(ConnectionIdTest, LessThanOperator) {
    // Test source IP difference
    stack::network::ConnectionId id1(asio::ip::make_address("1.1.1.1"), asio::ip::make_address("8.8.8.8"), 1000, 80);
    stack::network::ConnectionId id2(asio::ip::make_address("1.1.1.2"), asio::ip::make_address("8.8.8.8"), 1000, 80);
    EXPECT_LT(id1, id2);
    EXPECT_FALSE(id2 < id1);

    // Test source port difference
    stack::network::ConnectionId id3(asio::ip::make_address("1.1.1.1"), asio::ip::make_address("8.8.8.8"), 1000, 80);
    stack::network::ConnectionId id4(asio::ip::make_address("1.1.1.1"), asio::ip::make_address("8.8.8.8"), 1001, 80);
    EXPECT_LT(id3, id4);
    EXPECT_FALSE(id4 < id3);
}
