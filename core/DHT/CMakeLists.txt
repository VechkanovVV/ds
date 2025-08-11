cmake_minimum_required(VERSION 3.20)

project(dht LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

add_library(dht SHARED
        src/DHTNode.cpp
)

target_include_directories(dht PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/proto
        ${CMAKE_CURRENT_SOURCE_DIR}/../raft_node/src
)

target_link_libraries(dht PRIVATE
        raft_node
        protobuf::libprotobuf
        gRPC::grpc
        gRPC::grpc++
        gRPC::grpc++_reflection
        spdlog::spdlog
)