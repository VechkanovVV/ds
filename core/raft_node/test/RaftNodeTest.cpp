#include <gtest/gtest.h>
#include "../src/RaftNode.h"

// Вспомогательный класс-контекст для простоты (можно оставить пустым)
class DummyContext : public grpc::ServerContext {};

class RaftNodeTest : public ::testing::Test {
protected:
    // Два узла: id=1 и id=2, но с пустым списком peers (тест прямых вызовов)
    std::vector<Node> empty_peers{};
    RaftNode node1{1, empty_peers};
    RaftNode node2{2, empty_peers};

    RequestVoteRequest makeRequest(int term, int candidate_id) {
        RequestVoteRequest req;
        req.set_term(term);
        req.set_candidate_id(candidate_id);
        req.set_last_log_index(0);
        req.set_last_log_term(0);
        return req;
    }

    AppendEntriesRequest makeAppend(int term, int leader_id) {
        AppendEntriesRequest req;
        req.set_term(term);
        req.set_leader_id(leader_id);
        return req;
    }
};

TEST_F(RaftNodeTest, RequestVote_TooOldTerm) {
    RequestVoteResponse resp;
    // current_term_ у node1 == 0, пробуем запрос с term = -1 (условно старый)
    auto req = makeRequest(-1, /*candidate_id=*/2);
    DummyContext ctx;
    grpc::Status status = node1.RequestVote(&ctx, &req, &resp);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(resp.vote_granted(), false);
    EXPECT_EQ(resp.term(), 0);
}

TEST_F(RaftNodeTest, RequestVote_FirstVoteGranted) {
    RequestVoteResponse resp;
    // term > current_term_, node1 должен обновить term и дать голос
    auto req = makeRequest(1, /*candidate_id=*/2);
    DummyContext ctx;
    grpc::Status status = node1.RequestVote(&ctx, &req, &resp);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(resp.vote_granted(), true);
    EXPECT_EQ(resp.term(), 1);
}

TEST_F(RaftNodeTest, RequestVote_SecondVoteDenied) {
    // Сначала даём голос кандидату 2
    {
        RequestVoteResponse resp1;
        auto req1 = makeRequest(1, 2);
        DummyContext ctx1;
        node1.RequestVote(&ctx1, &req1, &resp1);
        ASSERT_TRUE(resp1.vote_granted());
    }
    // Попытка снова проголосовать за другого кандидата — голос не должен быть дан
    RequestVoteResponse resp2;
    auto req2 = makeRequest(1, /*candidate_id=*/3);
    DummyContext ctx2;
    grpc::Status status2 = node1.RequestVote(&ctx2, &req2, &resp2);

    EXPECT_TRUE(status2.ok());
    EXPECT_FALSE(resp2.vote_granted());
    EXPECT_EQ(resp2.term(), 1);
}

TEST_F(RaftNodeTest, AppendEntries_LeaderUpdatesTerm) {
    AppendEntriesResponse resp;
    // node2 current_term_ == 0, пришёл апдейт от лидера term=5
    auto req = makeAppend(5, /*leader_id=*/1);
    DummyContext ctx;
    grpc::Status status = node2.AppendEntries(&ctx, &req, &resp);

    EXPECT_TRUE(status.ok());
    EXPECT_TRUE(resp.success());
    EXPECT_EQ(resp.term(), 5);
}

// Точка входа для gtest
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
