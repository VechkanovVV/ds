#include <gtest/gtest.h>
#include <grpcpp/grpcpp.h>
#include "../src/RaftNode.h"
#include "../src/proto/raftnode.pb.h"
#include <thread>
#include <condition_variable>
#include <atomic>

class RaftNodeTest : public testing::Test {
protected:
    void SetUp() override {
        nodes = {
            {0, "127.0.0.1:50051"},
            {1, "127.0.0.1:50052"},
            {2, "127.0.0.1:50053"}
        };
    }

    void TearDown() override {
        for (auto& server : servers) {
            server->Shutdown();
        }
        for (auto& thread : server_threads) {
            if (thread.joinable()) thread.join();
        }
    }

    void StartServer(int id) {
        auto node = std::make_shared<RaftNode>(id, nodes);
        raft_nodes.push_back(node);

        grpc::ServerBuilder builder;
        builder.AddListeningPort(nodes[id].address, grpc::InsecureServerCredentials());
        builder.RegisterService(node.get());
        auto server = builder.BuildAndStart();
        servers.push_back(server);

        server_threads.emplace_back([server]() { server->Wait(); });
    }

    int FindLeaderId() const {
        for (size_t i = 0; i < raft_nodes.size(); ++i) {
            GetLeaderRequest request;
            GetLeaderResponse response;
            grpc::ClientContext context;

            auto channel = grpc::CreateChannel(
                nodes[i].address,
                grpc::InsecureChannelCredentials()
            );
            auto stub = RaftNodeService::NewStub(channel);

            if (grpc::Status status = stub->GetLeader(&context, request, &response); status.ok() && !response.leader_address().empty()) {
                for (const auto&[id, address] : nodes) {
                    if (address == response.leader_address()) {
                        return id;
                    }
                }
            }
        }
        return -1;
    }

    bool WaitForLeader(const int timeout_ms = 5000) const {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(timeout_ms)) {
            if (FindLeaderId() >= 0) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return false;
    }

    std::vector<Node> nodes;
    std::vector<std::shared_ptr<RaftNode>> raft_nodes;
    std::vector<std::unique_ptr<grpc::Server>> servers;
    std::vector<std::thread> server_threads;
};

TEST_F(RaftNodeTest, LeaderElection) {
    for (int i = 0; i < 3; ++i) {
        StartServer(i);
    }

    ASSERT_TRUE(WaitForLeader()) << "Leader not elected within timeout";

    int leader_count = 0;
    for (int i = 0; i < 3; ++i) {
        if (FindLeaderId() == i) leader_count++;
    }

    EXPECT_EQ(leader_count, 1) << "Should have exactly one leader";
}

TEST_F(RaftNodeTest, LogReplication) {
    for (int i = 0; i < 3; ++i) {
        StartServer(i);
    }

    ASSERT_TRUE(WaitForLeader());
    int leader_id = FindLeaderId();
    ASSERT_NE(leader_id, -1);

    auto channel = grpc::CreateChannel(
        nodes[leader_id].address,
        grpc::InsecureChannelCredentials()
    );
    auto stub = RaftNodeService::NewStub(channel);

    AppendEntriesRequest request;
    request.set_term(1);
    request.set_leader_id(leader_id);
    request.set_prev_log_index(-1);
    request.set_prev_log_term(-1);
    request.set_leader_commit(-1);

    LogEntry* entry = request.add_entries();
    entry->set_term(1);
    entry->set_data("test_data");

    AppendEntriesResponse response;
    grpc::ClientContext context;
    grpc::Status status = stub->AppendEntries(&context, request, &response);

    EXPECT_TRUE(status.ok());
    EXPECT_TRUE(response.success());

    for (int i = 0; i < 3; ++i) {
        SCOPED_TRACE("Checking node: " + std::to_string(i));

        auto node_stub = RaftNodeService::NewStub(
            grpc::CreateChannel(nodes[i].address, grpc::InsecureChannelCredentials())
        );

        AppendEntriesRequest probe;
        probe.set_term(1);
        probe.set_leader_id(leader_id);
        probe.set_prev_log_index(0);

        AppendEntriesResponse probe_res;
        grpc::ClientContext probe_ctx;

        if (grpc::Status probe_status = node_stub->AppendEntries(&probe_ctx, probe, &probe_res); probe_status.ok()) {
            EXPECT_FALSE(probe_res.success());
            EXPECT_EQ(probe_res.conflict_index(), 0);
        }
    }
}

TEST_F(RaftNodeTest, LeaderFailure) {
    for (int i = 0; i < 3; ++i) {
        StartServer(i);
    }

    ASSERT_TRUE(WaitForLeader());
    const int old_leader_id = FindLeaderId();
    ASSERT_NE(old_leader_id, -1);

    servers[old_leader_id]->Shutdown();
    raft_nodes[old_leader_id]->Shutdown();
    if (server_threads[old_leader_id].joinable()) {
        server_threads[old_leader_id].join();
    }

    ASSERT_TRUE(WaitForLeader(8000)) << "New leader not elected";
    const int new_leader_id = FindLeaderId();
    ASSERT_NE(new_leader_id, -1);
    EXPECT_NE(new_leader_id, old_leader_id);
}

TEST_F(RaftNodeTest, DenyVote) {
    StartServer(0);

    const auto channel = grpc::CreateChannel(
        nodes[0].address,
        grpc::InsecureChannelCredentials()
    );
    const auto stub = RaftNodeService::NewStub(channel);

    RequestVoteRequest request;
    request.set_term(3);
    request.set_candidate_id(1);
    request.set_last_log_index(0);
    request.set_last_log_term(0);

    RequestVoteResponse response;
    grpc::ClientContext context;
    const grpc::Status status = stub->RequestVote(&context, request, &response);

    EXPECT_TRUE(status.ok());
    if (status.ok()) {
        EXPECT_FALSE(response.vote_granted());
    }
}

TEST_F(RaftNodeTest, GetLeader) {
    for (int i = 0; i < 3; ++i) {
        StartServer(i);
    }

    ASSERT_TRUE(WaitForLeader());

    for (int i = 0; i < 3; ++i) {
        auto channel = grpc::CreateChannel(
            nodes[i].address,
            grpc::InsecureChannelCredentials()
        );
        const auto stub = RaftNodeService::NewStub(channel);

        GetLeaderRequest request;
        GetLeaderResponse response;
        grpc::ClientContext context;

        grpc::Status status = stub->GetLeader(&context, request, &response);
        EXPECT_TRUE(status.ok());
        if (status.ok()) {
            EXPECT_FALSE(response.leader_address().empty());
        }
    }
}