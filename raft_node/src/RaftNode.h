#pragma once

#include "proto/raftnode.grpc.pb.h"
#include "TimerDelegate.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <random>
#include <set>
#include <vector>
#include <string>
#include <memory>
#include <thread>
#include <grpcpp/grpcpp.h>
#include <functional>

class ElectionTimerStrategy;
class HeartbeatTimerStrategy;

enum class RaftNodeState { FOLLOWER, CANDIDATE, LEADER };

namespace Numbers {
constexpr int ElectionTimeoutMin = 150;
constexpr int HeartbeatTimeoutMin = 50;
};

struct Node {
    int id;
    std::string address;
};

class RaftNode final : public RaftNodeService::Service {
public:
    RaftNode(int id, const std::vector<Node>& peers, std::function<void(const std::string&)> apply_callback);
    ~RaftNode() override;

    // RPC методы Raft
    grpc::Status AppendEntries(grpc::ServerContext* context, const AppendEntriesRequest* request, AppendEntriesResponse* response) override;
    grpc::Status RequestVote(grpc::ServerContext* context, const RequestVoteRequest* request, RequestVoteResponse* response) override;
    grpc::Status GetLeader(grpc::ServerContext* context, const GetLeaderRequest* request, GetLeaderResponse* response) override;
    grpc::Status InstallSnapshot(grpc::ServerContext* context, const InstallSnapshotRequest* request, InstallSnapshotResponse* response) override;

    bool Submit(const std::string& command);
    std::string GetLeaderAddress() const;
    void Shutdown();

private:
    friend class ElectionTimerStrategy;
    friend class HeartbeatTimerStrategy;

    void StartElection();
    void SendHeartbeat();
    void FindLeader();
    void ApplyCommittedEntries();

    static std::unique_ptr<RaftNodeService::Stub> GetStub(const std::string& address);

    TimerDelegate timer_delegate_;

    const int id_;
    int leader_id_ = -1;
    std::vector<Node> peers_;
    mutable std::mutex state_mutex_;
    int current_term_ = 0;
    int voted_for_ = -1;
    std::set<int> votes_;
    std::atomic<RaftNodeState> state_ = RaftNodeState::FOLLOWER;

    // Лог и индексы
    std::vector<LogEntry> log_; // лог записей
    int commit_index_ = -1; // индекс последней коммитнутой записи
    int last_applied_ = -1; // индекс последней примененной записи

    // Только для лидера
    std::vector<int> next_index_; // для каждого последователя: следующий индекс для отправки
    std::vector<int> match_index_; // для каждого последователя: последний подтвержденный индекс

    mutable std::mt19937 rng_;
    std::uniform_int_distribution<int> election_dist_;

    std::function<void(const std::string&)> apply_callback_;
};

class ElectionTimerStrategy final : public ITimerStrategy {
public:
    ElectionTimerStrategy(RaftNode* node, std::uniform_int_distribution<int> dist);
    int nextTimeout() override;
    void onTimeout() override;

private:
    RaftNode* node_;
    std::uniform_int_distribution<int> dist_;
    std::mt19937 rng_;
};

class HeartbeatTimerStrategy final : public ITimerStrategy {
public:
    explicit HeartbeatTimerStrategy(RaftNode* node);
    int nextTimeout() override;
    void onTimeout() override;

private:
    RaftNode* node_;
};

class NullTimerStrategy final : public ITimerStrategy {
public:
    int nextTimeout() override;
    void onTimeout() override;
};