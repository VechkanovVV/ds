cmake_minimum_required(VERSION 3.20)

project(raft_node LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

add_library(raft_node SHARED
        src/RaftNode.cpp
        src/TimerDelegate.cpp
)

target_include_directories(raft_node PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/proto
)

target_link_libraries(raft_node PRIVATE
        protobuf::libprotobuf
        gRPC::grpc
        gRPC::grpc++
        gRPC::grpc++_reflection
        spdlog::spdlog
)