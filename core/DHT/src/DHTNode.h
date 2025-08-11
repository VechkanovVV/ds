#pragma once

#include "proto/dht.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <map>
#include <string>
#include <memory>
#include <mutex>
#include "../../raft_node/src/RaftNode.h"

namespace dht {

    extern std::mutex storage_mutex_;

    class DHTNode final : public DHTService::Service {
    public:
        DHTNode(const int id, const std::vector<Node>& peers);
        ~DHTNode() override;

        grpc::Status Put(grpc::ServerContext* context, const PutRequest* request, PutResponse* response) override;
        grpc::Status Get(grpc::ServerContext* context, const GetRequest* request, GetResponse* response) override;
        grpc::Status Delete(grpc::ServerContext* context, const DeleteRequest* request, DeleteResponse* response) override;

    private:
        void apply(const std::string& data);
        std::unique_ptr<DHTService::Stub> getStubToLeader();

        RaftNode raft_;
        std::map<std::string, std::string> storage_;
    };

}