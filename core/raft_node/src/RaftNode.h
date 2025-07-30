#pragma once

#include "proto/raftnode.grpc.pb.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <random>
#include <set>
#include <vector>

enum class RaftNodeState { FOLLOWER, CANDIDATE, LEADER };

namespace  Numbers {
    constexpr  int ElectionNumber = 150;
    constexpr int HeartbeatNumber = 300;
};

struct Node {
  int id;
  std::string address;
};

class RaftNode final: public RaftNodeService::Service {
public:
  RaftNode(int id, const std::vector<Node>& peers);
  ~RaftNode() override;

  // RPC методы
  grpc::Status AppendEntries(grpc::ServerContext*, const AppendEntriesRequest*, AppendEntriesResponse*) override;
  grpc::Status RequestVote(grpc::ServerContext*, const RequestVoteRequest*, RequestVoteResponse*) override;
  grpc::Status GetLeader(grpc::ServerContext*, const GetLeaderRequest*, GetLeaderResponse*) override;
  grpc::Status InstallSnapshot(grpc::ServerContext*, const InstallSnapshotRequest*, InstallSnapshotResponse*) override;

private:
  void StartElectionTimer();
  void StartElection();
  void ResetElectionTimer();
  void StartHeartbeatTimer();
  void SendHeartbeat() const;
  void FindLeader();

  static std::unique_ptr<RaftNodeService::Stub> GetStub(const std::string& address);

  const int id_;
  int leader_id_ = -1;
  const std::vector<Node> peers_;
  mutable std::mutex state_mutex_;
  int current_term_ = 0;
  int voted_for_ = -1;
  std::set<int> votes_;
  RaftNodeState state_ = RaftNodeState::FOLLOWER;

  // Таймеры
  std::mutex election_timer_mutex_;
  std::condition_variable election_timer_cv_;
  std::atomic<bool> election_timer_reset_ = false;
  std::atomic<bool> election_timer_stop_ = false;

  std::mutex heartbeat_timer_mutex_;
  std::condition_variable heartbeat_timer_cv_;
  std::atomic<bool> heartbeat_timer_stop_ = false;
  std::atomic<bool> heartbeat_timer_reset_ = false;

  // Генератор случайных чисел
  mutable std::mt19937 rng_{std::random_device{}()};
  std::uniform_int_distribution<int> heartbeat_dist_{
      Numbers::HeartbeatNumber,
      2 * Numbers::HeartbeatNumber
  };
  std::uniform_int_distribution<int> election_dist_{
      Numbers::ElectionNumber,
      2 * Numbers::ElectionNumber
  };
};