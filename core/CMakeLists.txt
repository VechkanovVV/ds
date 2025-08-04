cmake_minimum_required(VERSION 3.20)
project(core LANGUAGES CXX)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_BINARY_DIR}/conan_toolchain.cmake" CACHE FILEPATH "")

add_subdirectory(common)
add_subdirectory(raft_node)
add_subdirectory(DHT)
