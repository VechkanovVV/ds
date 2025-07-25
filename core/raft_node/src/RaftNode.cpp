#include <random>
#include <thread>
#include <chrono>
#include <grpcpp/grpcpp.h>
#include "RaftNode.h"

RaftNode::RaftNode(const int id, const std::vector<Node> &peers)
    : id_(id), peers_(peers) {
    std::random_device rd;
    gen_.seed(rd());

    StartElectionTimer();
}

std::unique_ptr<RaftNodeService::Stub> RaftNode::GetStub(const std::string& address) {
    const std::shared_ptr<grpc::Channel> channel =
        grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    return RaftNodeService::NewStub(channel);
}

void RaftNode::StartHeartbeatTimer() {
    assert(state_ == RaftNodeState::LEADER);
    std::thread([this]() {
        while (!heartbeatTimerStop_ && state_ == RaftNodeState::LEADER) {
            std::unique_lock lock(mutexHeartbeatTimer_);
            int timeout = heartbeatDist_(gen_);
            cv_heartbeat_timer_.wait_for(lock, std::chrono::milliseconds(timeout), [this]{
                return heartbeatTimerStop_ || heartbeatTimerReset_;
            });
            if (heartbeatTimerStop_) break;

            if (heartbeatTimerReset_) {
                heartbeatTimerReset_ = false;
            } else {
                lock.unlock();
                SendHeartbeat();
            }
        }
    }).detach();
}

void RaftNode::SendHeartbeat() {
    int currentTerm;
    int nodeId;
    std::vector<Node> peersCopy;
    {
        std::lock_guard lock(stateMutex_);
        peersCopy = peers_;
        currentTerm = currentTerm_;
        nodeId = id_;
    }

    std::vector<std::thread> threads;

    for (const auto& neigh : peersCopy) {
        threads.emplace_back([this, &neigh, nodeId, currentTerm] {
            const auto stub = GetStub(neigh.address);
            AppendEntriesArgs request;
            request.set_leader_id(nodeId);
            request.set_leader_id(currentTerm);
            AppendEntriesResponse response;
            grpc::ClientContext context;
            auto resp = stub->AppendEntries(&context, request, &response);
        });
    }

    for (auto& t : threads) {
        t.join();
    }
}


void RaftNode::StartElectionTimer() {
    std::thread([this] {
        while (!electionTimerStop_) {
            std::unique_lock lock(mutexElectionTimer_);
            int timeout = electionDist_(gen_);

            cv_election_timer_.wait_for(
                lock,
                std::chrono::milliseconds(timeout),
                [this] { return electionTimerReset_ || electionTimerStop_; }
            );

            if (electionTimerStop_) break;

            if (electionTimerReset_) {
                electionTimerReset_ = false;
            } else {
                lock.unlock();
                StartElection();
            }
        }
    }).detach();
}

void RaftNode::StartElection() {
    int currentTerm;
    int nodeId;
    std::vector<Node> peersCopy;
    {
        std::lock_guard lock(stateMutex_);

        if (state_ == RaftNodeState::LEADER) return;

        currentTerm_++;
        currentTerm = currentTerm_;
        nodeId = id_;
        votes_.clear();
        votes_.insert(id_);
        state_ = RaftNodeState::CANDIDATE;
        votedFor_ = id_;
        peersCopy = peers_;
    }

    std::vector<std::thread> threads;
    for (const auto& neighbor : peersCopy) {
        threads.emplace_back([this, neighbor, currentTerm, nodeId] {
            const auto stub = GetStub(neighbor.address);
            RequestVoteArgs request;
            request.set_candidate_id(nodeId);
            request.set_candidate_term(currentTerm);
            RequestVoteResponse response;
            grpc::ClientContext context;
            auto status = stub->RequestVote(&context, request, &response);

            if (status.ok() && response.vote_result()) {
                std::lock_guard lock(stateMutex_);

                if (state_ == RaftNodeState::CANDIDATE &&
                    currentTerm_ == currentTerm) {
                    votes_.insert(neighbor.id);

                    if (votes_.size() * 2 > peers_.size() + 1) {
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

    if (state_ != RaftNodeState::LEADER) {
        state_ = RaftNodeState::FOLLOWER;
        ResetElectionTimer();
    }
}

void RaftNode::ResetElectionTimer() {
    {
        std::lock_guard lock(mutexElectionTimer_);
        electionTimerReset_ = true;
    }
    cv_election_timer_.notify_one();
}

//--grpc--
grpc::Status RaftNode::RequestVote(grpc::ServerContext* serverContext, const RequestVoteArgs* requestArgs, RequestVoteResponse* response) {
    std::lock_guard lock(stateMutex_);
    response->set_term(currentTerm_);

    if (requestArgs->candidate_term() < currentTerm_) {
        response->set_vote_result(false);
        return grpc::Status::OK;
    }

    if (requestArgs->candidate_term() > currentTerm_) {
        currentTerm_ = requestArgs->candidate_term();
        state_ = RaftNodeState::FOLLOWER;
        votedFor_ = -1;
    }

    bool canVote = (votedFor_ == -1 || votedFor_ == requestArgs->candidate_id());

    if (canVote) {
        votedFor_ = requestArgs->candidate_id();
        response->set_vote_result(true);
        ResetElectionTimer();
    } else {
        response->set_vote_result(false);
    }

    return grpc::Status::OK;
}

grpc::Status RaftNode::AppendEntries(grpc::ServerContext* context, const AppendEntriesArgs* append_entries_args, AppendEntriesResponse* append_entries_response) {
    if (append_entries_args->leader_term() < currentTerm_) {
        return grpc::Status::CANCELLED;
    }
    {
        std::lock_guard lock(stateMutex_);
        currentTerm_ = append_entries_args->leader_term();
        leader_id_ = append_entries_args->leader_id();
    }

    return grpc::Status::OK;
}

