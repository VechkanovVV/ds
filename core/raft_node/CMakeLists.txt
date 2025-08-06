cmake_minimum_required(VERSION 3.20)

project(raft_node LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# Подключаем toolchain файл
include("${CMAKE_SOURCE_DIR}/../build/conan/conan_toolchain.cmake")

# Подключаем зависимости
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(spdlog REQUIRED)

add_library(raft_node SHARED
        src/RaftNode.cpp
        src/TimerDelegate.cpp
        src/proto/raftnode.pb.cc
        src/proto/raftnode.grpc.pb.cc
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
