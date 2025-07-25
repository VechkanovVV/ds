cmake_minimum_required(VERSION 3.20)
project(raft_node LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include("${CMAKE_SOURCE_DIR}/../build/conan_toolchain.cmake")

find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

set(PROTO_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/proto/raftnode.pb.cc
)
set(GRPC_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/proto/raftnode.grpc.pb.cc
)

add_library(raft_node SHARED
        src/RaftNode.cpp
        ${PROTO_SRCS}
        ${GRPC_SRCS}
)

target_include_directories(raft_node PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/proto
        ${Protobuf_INCLUDE_DIRS}
)

target_link_libraries(raft_node PRIVATE
        protobuf::libprotobuf
        gRPC::grpc
        gRPC::gpr
        gRPC::grpc++
        gRPC::grpc++_reflection
)
