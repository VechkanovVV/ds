#pragma once

#include "proto/raftnode.grpc.pb.h"
#include <vector>
#include <mutex>
#include <condition_variable>
#include <set>
#include <random>

enum RaftNodeState {
    FOLLOWER,
    CANDIDATE,
    LEADER
};

enum Numbers {
    ElectionNumber = 150,
    HeartbeatNumber = 300

};

struct Node {
    int id;
    std::string address;
};

class RaftNode : public RaftNodeService::Service {
public:
    RaftNode(int id, const std::vector<Node>& peers);

    // RPC методы
    grpc::Status AppendEntries(grpc::ServerContext*, const AppendEntriesArgs*, AppendEntriesResponse*) override;
    grpc::Status RequestVote(grpc::ServerContext*, const RequestVoteArgs*, RequestVoteResponse*) override;
    grpc::Status GetLeader(grpc::ServerContext*, const GetLeaderArgs*, GetLeaderResponse*) override;
    grpc::Status AddValue(grpc::ServerContext*, const AddValueArgs*, AddValueResponse*) override;
    grpc::Status GetValue(grpc::ServerContext*, const GetValueArgs*, GetValueResponse*) override;
    grpc::Status Suspend(grpc::ServerContext*, const SuspendArgs*, SuspendResponse*) override;
    grpc::Status Resume(grpc::ServerContext*, const ResumeArgs*, ResumeResponse*) override;

private:
    void StartElectionTimer();
    void StartElection();
    void ResetElectionTimer();
    void StartHeartbeatTimer();
    void SendHeartbeat();

    std::unique_ptr<RaftNodeService::Stub> GetStub(const std::string& address);

    const int id_;
    int leader_id_;
    const std::vector<Node> peers_;
    std::mutex stateMutex_;
    int currentTerm_ = 0;
    int votedFor_ = -1;
    std::set<int> votes_;
    RaftNodeState state_ = RaftNodeState::FOLLOWER;

    std::mutex mutexElectionTimer_;
    std::condition_variable cv_election_timer_;
    std::mutex mutexHeartbeatTimer_;
    std::condition_variable cv_heartbeat_timer_;
    std::atomic<bool> electionTimerReset_ = false;
    std::atomic<bool> electionTimerStop_ = false;
    std::atomic<bool> heartbeatTimerStop_ = false;
    std::atomic<bool> heartbeatTimerReset_ = false;
    std::mt19937 gen_{std::random_device{}()};
    std::uniform_int_distribution<int> heartbeatDist_{
        Numbers::HeartbeatNumber,
        2 * Numbers::HeartbeatNumber
    };
    std::uniform_int_distribution<int> electionDist_{
        Numbers::ElectionNumber,
        2 * Numbers::ElectionNumber
    };
};