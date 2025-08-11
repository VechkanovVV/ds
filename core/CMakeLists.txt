cmake_minimum_required(VERSION 3.20)
project(core LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/build/conan/conan_toolchain.cmake" CACHE FILEPATH "Conan toolchain")

find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(spdlog REQUIRED)

add_subdirectory(raft_node)
add_subdirectory(DHT)

add_executable(core src/main.cpp)
target_link_libraries(core PRIVATE dht raft_node protobuf::libprotobuf gRPC::grpc gRPC::grpc++ gRPC::grpc++_reflection spdlog::spdlog)
target_include_directories(core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/DHT/src ${CMAKE_CURRENT_SOURCE_DIR}/raft_node/src)