#include "RaftNode.h"
#include <thread>
#include <grpcpp/grpcpp.h>
#include <algorithm>

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

void RaftNode::ApplyCommittedEntries() {
    // Применяем все коммиты, которые еще не были применены
    while (last_applied_ < commit_index_) {
        last_applied_++;
        // TODO: Применить команду log_[last_applied_].data() к состоянию (DHT)
        // Например: dht_.apply(log_[last_applied_].data());
    }
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
    std::vector<int> next_index_copy;
    std::vector<int> match_index_copy;
    
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (state_ != RaftNodeState::LEADER) return;
        
        peers_copy = peers_;
        next_index_copy = next_index_;
        match_index_copy = match_index_;
        current_term = current_term_;
        node_id = id_;
    }

    std::vector<std::thread> threads;
    for (size_t i = 0; i < peers_copy.size(); ++i) {
        if (peers_copy[i].id == id_) continue;
        
        threads.emplace_back([this, i, peers_copy, next_index_copy, match_index_copy, node_id, current_term] {
            const auto stub = GetStub(peers_copy[i].address);
            AppendEntriesRequest request;
            request.set_term(current_term);
            request.set_leader_id(node_id);
            request.set_leader_commit(commit_index_);
            
            // Формируем запрос на основе next_index_copy[i]
            int prev_log_index = next_index_copy[i] - 1;
            int prev_log_term = -1;
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                if (prev_log_index >= 0) {
                    if (prev_log_index < log_.size()) {
                        prev_log_term = log_[prev_log_index].term();
                    } else {
                        // Несоответствие: отправим снапшот (пока просто выходим)
                        return;
                    }
                }
            }
            request.set_prev_log_index(prev_log_index);
            request.set_prev_log_term(prev_log_term);
            
            // Добавляем записи, начиная с next_index_copy[i]
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                for (int j = next_index_copy[i]; j < log_.size(); ++j) {
                    auto* entry = request.add_entries();
                    entry->set_term(log_[j].term());
                    entry->set_data(log_[j].data());
                }
            }

            AppendEntriesResponse response;
            grpc::ClientContext context;

            if (stub->AppendEntries(&context, request, &response).ok()) {
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
                    return;
                }
                
                if (response.success()) {
                    // Успешно реплицировали записи
                    next_index_[i] = next_index_copy[i] + request.entries_size();
                    match_index_[i] = next_index_[i] - 1;
                    
                    // Проверяем, можно ли обновить commit_index
                    std::vector<int> match_indices;
                    for (int index : match_index_) {
                        match_indices.push_back(index);
                    }
                    match_indices.push_back(log_.size() - 1); // Лидер учитывает свой лог
                    std::sort(match_indices.begin(), match_indices.end());
                    int new_commit_index = match_indices[match_indices.size() / 2];
                    
                    if (new_commit_index > commit_index_ && log_[new_commit_index].term() == current_term_) {
                        commit_index_ = new_commit_index;
                        ApplyCommittedEntries(); // Применяем коммиты
                    }
                } else {
                    // Конфликт: уменьшаем next_index
                    next_index_[i] = std::max(0, next_index_copy[i] - 1);
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
            request.set_last_log_index(static_cast<int>(log_.size()) - 1);
            request.set_last_log_term(
                (log_.empty() ? 0 : log_.back().term())
            );

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

                        // Инициализация состояния лидера
                        next_index_.resize(peers_.size(), log_.size());
                        match_index_.resize(peers_.size(), -1);
                        
                        timer_delegate_.changeStrategy(
                            std::make_unique<HeartbeatTimerStrategy>(this)
                        );
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
        
        // Проверка лога
        bool log_ok = true;
        if (!log_.empty()) {
            int last_index = log_.size() - 1;
            int last_term = log_.back().term();
            
            log_ok = (request->last_log_term() > last_term) ||
                     (request->last_log_term() == last_term && 
                      request->last_log_index() >= last_index);
        }

        if ((voted_for_ == -1 || voted_for_ == request->candidate_id()) && log_ok) {
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

    // Обновление термина и сброс состояния
    if (request->term() > current_term_) {
        current_term_ = request->term();
        state_ = RaftNodeState::FOLLOWER;
        voted_for_ = -1;
        leader_id_ = request->leader_id();
    } else {
        leader_id_ = request->leader_id();
    }

    timer_delegate_.reset(); // Сброс таймера выборов

    // 1. Проверка prev_log_index и prev_log_term
    bool log_ok = true;
    if (request->prev_log_index() >= 0) {
        if (request->prev_log_index() >= log_.size()) {
            log_ok = false;
        } else if (log_[request->prev_log_index()].term() != request->prev_log_term()) {
            log_ok = false;
        }
    }
    
    if (!log_ok) {
        response->set_success(false);
        return grpc::Status::OK;
    }

    // 2. Вставка новых записей (с удалением конфликтов)
    int insert_index = request->prev_log_index() + 1;
    if (insert_index < log_.size()) {
        // Удаляем конфликтующие записи
        log_.erase(log_.begin() + insert_index, log_.end());
    }
    
    // Добавляем новые записи
    for (const auto& entry : request->entries()) {
        log_.push_back(entry);
    }

    // 3. Обновляем commit_index
    if (request->leader_commit() > commit_index_) {
        commit_index_ = std::min(request->leader_commit(), static_cast<int>(log_.size() - 1));
        ApplyCommittedEntries();
    }

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
ElectionTimerStrategy::ElectionTimerStrategy(RaftNode* node, std::uniform_int_distribution<int> dist)
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