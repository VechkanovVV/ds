#include "DHTNode.h"
#include "proto/dht.pb.h"
#include <grpcpp/grpcpp.h>
#include <functional>
#include <spdlog/spdlog.h>
#include <mutex>

namespace dht {

std::mutex storage_mutex_;

DHTNode::DHTNode(const int id, const std::vector<Node>& peers)
    : raft_(id, peers, [this](const std::string& data) { apply(data); }) {
    spdlog::info("DHTNode {} initialized", id);
}

DHTNode::~DHTNode() {
    raft_.Shutdown();
    spdlog::info("DHTNode destructed");
}

void DHTNode::apply(const std::string& data) {
    Command cmd;
    if (!cmd.ParseFromString(data)) {
        spdlog::error("Failed to parse command");
        return;
    }
    std::lock_guard<std::mutex> lock(storage_mutex_);
    switch (cmd.type()) {
        case Command::PUT:
            storage_[cmd.key()] = cmd.value();
            spdlog::debug("Applied PUT: key={}", cmd.key());
            break;
        case Command::DELETE:
            storage_.erase(cmd.key());
            spdlog::debug("Applied DELETE: key={}", cmd.key());
            break;
        default:
            spdlog::warn("Unknown command type");
            break;
    }
}

std::unique_ptr<DHTService::Stub> DHTNode::getStubToLeader() {
    std::string leader_addr = raft_.GetLeaderAddress();
    if (leader_addr.empty()) {
        spdlog::warn("Leader address not found");
        return nullptr;
    }
    spdlog::debug("Connecting to leader at {}", leader_addr);
    auto channel = grpc::CreateChannel(leader_addr, grpc::InsecureChannelCredentials());
    return DHTService::NewStub(channel);
}

grpc::Status DHTNode::Put(grpc::ServerContext* context, const PutRequest* request, PutResponse* response) {
    spdlog::info("Received Put request for key={}", request->key());
    Command cmd;
    cmd.set_type(Command::PUT);
    cmd.set_key(request->key());
    cmd.set_value(request->value());
    std::string data;
    cmd.SerializeToString(&data);

    {
        std::lock_guard<std::mutex> lock(raft_.state_mutex_);
        if (raft_.state_ == RaftNodeState::LEADER) {
            bool success = raft_.Submit(data);
            response->set_success(success);
            if (success) spdlog::info("Submitted Put to Raft");
            else spdlog::warn("Failed to submit Put to Raft");
            return grpc::Status::OK;
        }
    }

    auto stub = getStubToLeader();
    if (!stub) {
        spdlog::error("Leader not found for Put redirect");
        return grpc::Status(grpc::NOT_FOUND, "Leader not found");
    }
    grpc::ClientContext ctx;
    return stub->Put(&ctx, *request, response);
}

grpc::Status DHTNode::Get(grpc::ServerContext* context, const GetRequest* request, GetResponse* response) {
    spdlog::info("Received Get request for key={}", request->key());
    {
        std::lock_guard<std::mutex> lock(raft_.state_mutex_);
        if (raft_.state_ == RaftNodeState::LEADER) {
            std::lock_guard<std::mutex> storage_lock(storage_mutex_);
            auto it = storage_.find(request->key());
            if (it != storage_.end()) {
                response->set_value(it->second);
                response->set_found(true);
                spdlog::debug("Found value for key={}", request->key());
            } else {
                response->set_found(false);
                spdlog::debug("Key={} not found", request->key());
            }
            return grpc::Status::OK;
        }
    }

    auto stub = getStubToLeader();
    if (!stub) {
        spdlog::error("Leader not found for Get redirect");
        return grpc::Status(grpc::NOT_FOUND, "Leader not found");
    }
    grpc::ClientContext ctx;
    return stub->Get(&ctx, *request, response);
}

grpc::Status DHTNode::Delete(grpc::ServerContext* context, const DeleteRequest* request, DeleteResponse* response) {
    spdlog::info("Received Delete request for key={}", request->key());
    Command cmd;
    cmd.set_type(Command::DELETE);
    cmd.set_key(request->key());
    std::string data;
    cmd.SerializeToString(&data);

    {
        std::lock_guard<std::mutex> lock(raft_.state_mutex_);
        if (raft_.state_ == RaftNodeState::LEADER) {
            bool success = raft_.Submit(data);
            response->set_success(success);
            if (success) spdlog::info("Submitted Delete to Raft");
            else spdlog::warn("Failed to submit Delete to Raft");
            return grpc::Status::OK;
        }
    }

    auto stub = getStubToLeader();
    if (!stub) {
        spdlog::error("Leader not found for Delete redirect");
        return grpc::Status(grpc::NOT_FOUND, "Leader not found");
    }
    grpc::ClientContext ctx;
    return stub->Delete(&ctx, *request, response);
}

}