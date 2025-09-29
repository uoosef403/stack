#include <gtest/gtest.h>
#include <gmock/gmock.h>

// Example test to verify Google Test is working
TEST(BasicTest, SampleTest) {
    EXPECT_EQ(1 + 1, 2);
    EXPECT_TRUE(true);
}

// Example test for string operations
TEST(StringTest, BasicStringOperations) {
    std::string hello = "Hello";
    std::string world = "World";
    std::string result = hello + " " + world;
    
    EXPECT_EQ(result, "Hello World");
    EXPECT_THAT(result, testing::HasSubstr("Hello"));
    EXPECT_THAT(result, testing::HasSubstr("World"));
}

// Example parameterized test
class MathTest : public testing::TestWithParam<std::pair<int, int>> {};

TEST_P(MathTest, Addition) {
    auto params = GetParam();
    int a = params.first;
    int b = params.second;
    
    EXPECT_EQ(a + b, b + a); // Commutative property
}

INSTANTIATE_TEST_SUITE_P(
    AdditionTests,
    MathTest,
    testing::Values(
        std::make_pair(1, 2),
        std::make_pair(5, 10),
        std::make_pair(-1, 1),
        std::make_pair(0, 0)
    )
);
