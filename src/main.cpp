#include <grpcpp/grpcpp.h>
#include <spdlog/spdlog.h>
#include "DHTNode.h"
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <id> <address> [<peer_id> <peer_address>]..." << std::endl;
        return 1;
    }
    spdlog::set_default_logger(spdlog::stdout_color_logger_mt("core"));
    spdlog::set_level(spdlog::level::info);
    int id = std::stoi(argv[1]);
    std::string address = argv[2];
    std::vector<::Node> peers;
    peers.push_back({id, address});
    for (int i = 3; i < argc; i += 2) {
        if (i + 1 >= argc) break;
        peers.push_back({std::stoi(argv[i]), argv[i + 1]});
    }
    spdlog::info("Starting node {} at {}", id, address);
    dht::DHTNode node(id, peers);
    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&node);
    builder.RegisterService(&node.raft_);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    spdlog::info("Server listening on {}", address);
    server->Wait();
    return 0;
}