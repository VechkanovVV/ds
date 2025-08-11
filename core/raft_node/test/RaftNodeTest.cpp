#include <gtest/gtest.h>
#include "../src/RaftNode.h"

// Пример теста (расширь по нужде)
TEST(RaftNodeTest, Basic) {
    std::vector<Node> peers = {{1, "localhost:50051"}};
    RaftNode node(1, peers, [](const std::string&) {});
    EXPECT_EQ(node.GetLeaderAddress(), "");  // Initially no leader
}

// Запуск: gtest в CMake добавить add_test или отдельно компилировать