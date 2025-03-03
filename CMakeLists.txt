cmake_minimum_required(VERSION 3.20)
project(tor)

set(CMAKE_CXX_STANDARD 20)

find_package(Boost 1.75 REQUIRED COMPONENTS system)
find_package(SQLite3 REQUIRED)

add_executable(tor
    src/main.cpp
)

target_link_libraries(tor
    PRIVATE Boost::boost Boost::system SQLite::SQLite3
)