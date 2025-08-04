cmake_minimum_required(VERSION 3.20)
project(RaftNode VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include("${CMAKE_SOURCE_DIR}/../build/conan/conan_toolchain.cmake")

find_package(Protobuf REQUIRED)
find_package(gRPC      REQUIRED)
find_package(GTest     REQUIRED)

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

target_include_directories(raft_node
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/proto>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(raft_node
        PUBLIC
        protobuf::libprotobuf
        gRPC::grpc
        gRPC::gpr
        gRPC::grpc++
        gRPC::grpc++_reflection
)
