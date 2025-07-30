#include <random>
#include <thread>
#include <chrono>
#include <grpcpp/grpcpp.h>
#include "RaftNode.h"

RaftNode::RaftNode(const int id, const std::vector<Node>& peers)
    : id_(id), peers_(peers) {
  StartElectionTimer();
}

RaftNode::~RaftNode() {
  election_timer_stop_.store(true);
  election_timer_cv_.notify_one();

  heartbeat_timer_stop_.store(true);
  heartbeat_timer_cv_.notify_one();
}

std::unique_ptr<RaftNodeService::Stub> RaftNode::GetStub(const std::string& address) {
  const auto channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
  return RaftNodeService::NewStub(channel);
}

void RaftNode::FindLeader() {
  std::vector<Node> peers;
  {
    std::lock_guard lock(state_mutex_);
    peers = peers_;
  }

  std::vector<std::thread> threads;
  for (const auto& p : peers) {
    threads.emplace_back([this, p]{
      const auto stub = GetStub(p.address);
      const GetLeaderRequest get_leader_request;
      GetLeaderResponse get_leader_response;
      grpc::ClientContext context;
      if (const auto resp = stub->GetLeader(&context, get_leader_request, &get_leader_response); resp.ok()) {
        {
          std::lock_guard lock(state_mutex_);
          leader_id_ = -1;
          const std::string& leader_address = get_leader_response.leader_address();
          for (const auto&[id, address] : peers_) {
            if (address == leader_address) {
              leader_id_ = id;
              break;
            }
          }
        }
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }
}

void RaftNode::StartHeartbeatTimer() {
  assert(state_ == RaftNodeState::LEADER);
  std::thread heartbeat_thread([this] {
    while (!heartbeat_timer_stop_ && state_ == RaftNodeState::LEADER) {
      std::unique_lock lock(heartbeat_timer_mutex_);
      const int timeout = heartbeat_dist_(rng_);
      const bool timeout_expired = !heartbeat_timer_cv_.wait_for(
          lock, std::chrono::milliseconds(timeout), [this] {
            return heartbeat_timer_stop_ || heartbeat_timer_reset_;
          });

      if (heartbeat_timer_stop_) break;

      if (heartbeat_timer_reset_) {
        heartbeat_timer_reset_ = false;
      } else if (timeout_expired && state_ == RaftNodeState::LEADER) {
        lock.unlock();
        SendHeartbeat();
      }
    }
  });
  heartbeat_thread.detach();
}

void RaftNode::SendHeartbeat() const {
  int current_term;
  int node_id;
  std::vector<Node> peers_copy;
  {
    std::lock_guard lock(state_mutex_);
    peers_copy = peers_;
    current_term = current_term_;
    node_id = id_;
  }

  std::vector<std::thread> threads;
  threads.reserve(peers_copy.size());

  for (const auto& neighbor : peers_copy) {
    threads.emplace_back([this, neighbor, node_id, current_term] {
      const auto stub = GetStub(neighbor.address);
      AppendEntriesRequest request;
      request.set_term(current_term);
      request.set_leader_id(node_id);
      // Остальные поля для heartbeat не нужны
      AppendEntriesResponse response;
      grpc::ClientContext context;
      stub->AppendEntries(&context, request, &response);
    });
  }

  for (auto& t : threads) {
    t.join();
  }
}

void RaftNode::StartElectionTimer() {
  std::thread election_thread([this] {
    while (!election_timer_stop_) {
      std::unique_lock lock(election_timer_mutex_);
      const int timeout = election_dist_(rng_);
      const bool timeout_expired = !election_timer_cv_.wait_for(
          lock, std::chrono::milliseconds(timeout), [this] {
            return election_timer_stop_ || election_timer_reset_;
          });

      if (election_timer_stop_) break;

      if (election_timer_reset_) {
        election_timer_reset_ = false;
      } else if (timeout_expired) {
        lock.unlock();
        StartElection();
      }
    }
  });
  election_thread.detach();
}

void RaftNode::StartElection() {
  int current_term;
  int node_id;
  std::vector<Node> peers_copy;
  {
    std::lock_guard lock(state_mutex_);

    if (state_ != RaftNodeState::FOLLOWER) return;

    current_term_++;
    current_term = current_term_;
    node_id = id_;
    votes_.clear();
    votes_.insert(id_);
    state_ = RaftNodeState::CANDIDATE;
    voted_for_ = id_;
    peers_copy = peers_;
  }

  std::vector<std::thread> threads;
  threads.reserve(peers_copy.size());

  for (const auto& neighbor : peers_copy) {
    threads.emplace_back([this, neighbor, current_term, node_id] {
      const auto stub = GetStub(neighbor.address);
      RequestVoteRequest request;
      request.set_term(current_term);
      request.set_candidate_id(node_id);
      // Заглушка для лога (пока не реализовано)
      request.set_last_log_index(0);
      request.set_last_log_term(0);

      RequestVoteResponse response;
      grpc::ClientContext context;
      if (const auto status = stub->RequestVote(&context, request, &response); status.ok() && response.vote_granted()) {
        std::lock_guard lock(state_mutex_);

        if (state_ == RaftNodeState::CANDIDATE &&
            current_term_ == current_term) {
          votes_.insert(neighbor.id);
          const size_t majority = (peers_.size() + 1) / 2 + 1;

          if (votes_.size() >= majority) {
            state_ = RaftNodeState::LEADER;
            StartHeartbeatTimer();
          }
        }
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }
}

void RaftNode::ResetElectionTimer() {
  {
    std::lock_guard lock(election_timer_mutex_);
    election_timer_reset_ = true;
  }
  election_timer_cv_.notify_one();
}

grpc::Status RaftNode::RequestVote(grpc::ServerContext*, const RequestVoteRequest* request, RequestVoteResponse* response) {
  std::lock_guard lock(state_mutex_);

  response->set_term(current_term_);

  if (request->term() < current_term_) {
    response->set_vote_granted(false);
    return grpc::Status::OK;
  }

  if (request->term() > current_term_) {
    current_term_ = request->term();
    state_ = RaftNodeState::FOLLOWER;
    voted_for_ = -1;
  }

  if (voted_for_ == -1 || voted_for_ == request->candidate_id()) {
    voted_for_ = request->candidate_id();
    response->set_vote_granted(true);
    ResetElectionTimer();
  } else {
    response->set_vote_granted(false);
  }

  return grpc::Status::OK;
}

grpc::Status RaftNode::AppendEntries(grpc::ServerContext* context, const AppendEntriesRequest* request, AppendEntriesResponse* response) {
  std::lock_guard lock(state_mutex_);

  if (request->term() < current_term_) {
    response->set_term(current_term_);
    response->set_success(false);
    return grpc::Status::OK;
  }

  if (request->term() > current_term_) {
    current_term_ = request->term();
    state_ = RaftNodeState::FOLLOWER;
  }

  leader_id_ = request->leader_id();
  ResetElectionTimer();

  response->set_term(current_term_);
  response->set_success(true);
  return grpc::Status::OK;
}

grpc::Status RaftNode::GetLeader(grpc::ServerContext* context, const GetLeaderRequest*, GetLeaderResponse* response) {
  if (state_ == RaftNodeState::LEADER) {
    for (const auto& node : peers_) {
      if (node.id == id_) {
        response->set_leader_address(node.address);
        return grpc::Status::OK;
      }
    }
  } else if (leader_id_ != -1) {
    for (const auto&[id, address] : peers_) {
      if (id == leader_id_) {
        response->set_leader_address(address);
        return grpc::Status::OK;
      }
    }
  }

  return grpc::Status(grpc::StatusCode::NOT_FOUND, "Leader not found");
}

grpc::Status RaftNode::InstallSnapshot(grpc::ServerContext* context, const InstallSnapshotRequest* request, InstallSnapshotResponse* response) {
  std::lock_guard lock(state_mutex_);

  if (request->term() < current_term_) {
    response->set_term(current_term_);
    return grpc::Status::OK;
  }

  if (request->term() > current_term_) {
    current_term_ = request->term();
    state_ = RaftNodeState::FOLLOWER;
  }

  // Здесь должна быть логика установки снапшота
  // Пока заглушка
  ResetElectionTimer();

  response->set_term(current_term_);
  return grpc::Status::OK;
}