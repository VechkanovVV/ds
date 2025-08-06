#include "RaftNode.h"
#include <thread>
#include <grpcpp/grpcpp.h>

RaftNode::RaftNode(const int id, const std::vector<Node>& peers)
    : id_(id),
      peers_(peers),
      rng_(std::random_device{}()),
      election_dist_(Numbers::ElectionTimeoutMin, 2 * Numbers::ElectionTimeoutMin)
{
    timer_delegate_.start(std::make_unique<ElectionTimerStrategy>(this, election_dist_));
}

RaftNode::~RaftNode() {
    timer_delegate_.stop();
}

std::unique_ptr<RaftNodeService::Stub> RaftNode::GetStub(const std::string& address) {
    const auto channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    return RaftNodeService::NewStub(channel);
}

void RaftNode::FindLeader() {
    std::vector<Node> peers_copy;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        peers_copy = peers_;
    }

    std::vector<std::thread> threads;
    for (const auto& p : peers_copy) {
        threads.emplace_back([this, p]{
            const auto stub = GetStub(p.address);
            const GetLeaderRequest get_leader_request;
            GetLeaderResponse get_leader_response;
            grpc::ClientContext context;
            if (const auto status = stub->GetLeader(&context, get_leader_request, &get_leader_response); status.ok()) {
                std::lock_guard<std::mutex> lock(state_mutex_);
                leader_id_ = -1;
                const std::string& leader_address = get_leader_response.leader_address();
                for (const auto& [id, address] : peers_) {
                    if (address == leader_address) {
                        leader_id_ = id;
                        break;
                    }
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }
}

void RaftNode::SendHeartbeat() {
    int current_term;
    int node_id;
    std::vector<Node> peers_copy;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (state_ != RaftNodeState::LEADER) return;
        peers_copy = peers_;
        current_term = current_term_;
        node_id = id_;
    }

    std::vector<std::thread> threads;
    for (const auto& neighbor : peers_copy) {
        if (neighbor.id == id_) continue;

        threads.emplace_back([this, neighbor, node_id, current_term] {
            const auto stub = GetStub(neighbor.address);
            AppendEntriesRequest request;
            request.set_term(current_term);
            request.set_leader_id(node_id);
            AppendEntriesResponse response;
            grpc::ClientContext context;

            if (stub->AppendEntries(&context, request, &response).ok()) {
                if (response.term() > current_term) {
                    std::lock_guard<std::mutex> lock(state_mutex_);
                    if (response.term() > current_term_) {
                        current_term_ = response.term();
                        state_ = RaftNodeState::FOLLOWER;
                        voted_for_ = -1;
                        leader_id_ = -1;
                        timer_delegate_.changeStrategy(
                            std::make_unique<ElectionTimerStrategy>(this, election_dist_)
                        );
                        timer_delegate_.reset();
                    }
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }
}

void RaftNode::StartElection() {
    int current_term, node_id;
    std::vector<Node> peers_copy;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (state_ != RaftNodeState::FOLLOWER) return;

        state_ = RaftNodeState::CANDIDATE;
        current_term_++;
        current_term = current_term_;
        node_id = id_;
        votes_.clear();
        votes_.insert(id_);
        voted_for_ = id_;
        peers_copy = peers_;

        // Смена стратегии таймера
        timer_delegate_.changeStrategy(
            std::make_unique<NullTimerStrategy>()
        );
    }

    // TODO: логгируем начало выборов

    std::vector<std::thread> threads;
    for (const auto& neighbor : peers_copy) {
        if (neighbor.id == id_) continue;

        threads.emplace_back([this, neighbor, current_term, node_id] {
            const auto stub = GetStub(neighbor.address);
            RequestVoteRequest request;
            request.set_term(current_term);
            request.set_candidate_id(node_id);
            request.set_last_log_index(0); // TODO: реальные значения
            request.set_last_log_term(0);

            RequestVoteResponse response;
            grpc::ClientContext context;

            if (stub->RequestVote(&context, request, &response).ok() && response.vote_granted()) {
                std::lock_guard<std::mutex> lock(state_mutex_);
                if (state_ == RaftNodeState::CANDIDATE && current_term_ == current_term) {
                    votes_.insert(neighbor.id);
                    if (votes_.size() >= peers_.size() / 2 + 1) {
                        state_ = RaftNodeState::LEADER;
                        leader_id_ = id_;
                        // TODO: логгировать избрание

                        // Смена на heartbeat стратегию
                        timer_delegate_.changeStrategy(
                            std::make_unique<HeartbeatTimerStrategy>(this)
                        );
                        // Немедленная отправка heartbeat
                        SendHeartbeat();
                    }
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Обработка неудачных выборов
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (state_ == RaftNodeState::CANDIDATE) {
            state_ = RaftNodeState::FOLLOWER;
            timer_delegate_.changeStrategy(
                std::make_unique<ElectionTimerStrategy>(this, election_dist_)
            );
            timer_delegate_.reset();
        }
    }
}

grpc::Status RaftNode::RequestVote(grpc::ServerContext*, const RequestVoteRequest* request, RequestVoteResponse* response) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    response->set_term(current_term_);
    bool vote_granted = false;

    if (request->term() < current_term_) {
        vote_granted = false;
    } else {
        if (request->term() > current_term_) {
            current_term_ = request->term();
            state_ = RaftNodeState::FOLLOWER;
            voted_for_ = -1;
            leader_id_ = -1;
        }
        if ((voted_for_ == -1 || voted_for_ == request->candidate_id())) {
            // TODO: проверка лога
            voted_for_ = request->candidate_id();
            vote_granted = true;
            timer_delegate_.reset();
        }
    }

    response->set_vote_granted(vote_granted);
    return grpc::Status::OK;
}

grpc::Status RaftNode::AppendEntries(grpc::ServerContext*, const AppendEntriesRequest* request, AppendEntriesResponse* response) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    response->set_term(current_term_);
    if (request->term() < current_term_) {
        response->set_success(false);
        return grpc::Status::OK;
    }

    // Обновление термина
    if (request->term() > current_term_) {
        current_term_ = request->term();
        state_ = RaftNodeState::FOLLOWER;
        voted_for_ = -1;
    }

    leader_id_ = request->leader_id();
    timer_delegate_.reset();

    // TODO: обработка лога
    response->set_success(true);
    return grpc::Status::OK;
}

grpc::Status RaftNode::GetLeader(grpc::ServerContext*, const GetLeaderRequest*, GetLeaderResponse* response) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (state_ == RaftNodeState::LEADER) {
        for (const auto& [id, address] : peers_) {
            if (id == id_) {
                response->set_leader_address(address);
                return grpc::Status::OK;
            }
        }
    }
    if (leader_id_ != -1) {
        for (const auto& [id, address] : peers_) {
            if (id == leader_id_) {
                response->set_leader_address(address);
                return grpc::Status::OK;
            }
        }
    }
    return grpc::Status(grpc::StatusCode::NOT_FOUND, "Leader not found");
}

grpc::Status RaftNode::InstallSnapshot(grpc::ServerContext*, const InstallSnapshotRequest* request, InstallSnapshotResponse* response) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (request->term() < current_term_) {
        response->set_term(current_term_);
        return grpc::Status::OK;
    }
    if (request->term() > current_term_) {
        current_term_ = request->term();
        state_ = RaftNodeState::FOLLOWER;
        voted_for_ = -1;
        leader_id_ = -1;
    }

    // TODO: обработка снапшота
    timer_delegate_.reset();
    response->set_term(current_term_);
    return grpc::Status::OK;
}

// Реализации стратегий таймеров
ElectionTimerStrategy::ElectionTimerStrategy(RaftNode* node, const std::uniform_int_distribution<int> dist)
    : node_(node), dist_(dist), rng_(std::random_device{}()) {}

int ElectionTimerStrategy::nextTimeout() {
    return dist_(rng_);
}

void ElectionTimerStrategy::onTimeout() {
    node_->StartElection();
}

HeartbeatTimerStrategy::HeartbeatTimerStrategy(RaftNode* node)
    : node_(node) {}

int HeartbeatTimerStrategy::nextTimeout() {
    return Numbers::HeartbeatTimeoutMin;
}

void HeartbeatTimerStrategy::onTimeout() {
    node_->SendHeartbeat();
}

int NullTimerStrategy::nextTimeout() {
    return -1;
}

void NullTimerStrategy::onTimeout() {}