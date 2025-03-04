cmake_minimum_required(VERSION 3.20)
project(tor)

set(CMAKE_CXX_STANDARD 20)

# Поиск зависимостей
find_package(Boost 1.75 REQUIRED COMPONENTS system)
find_package(SQLite3 REQUIRED)

# Для readline
find_library(READLINE_LIB readline)
find_library(HISTORY_LIB history)
find_library(NCURSES_LIB ncurses)

add_executable(tor
    src/main.cpp
    src/peer.cpp
    src/network_manager.cpp
    src/cli.cpp
)

# Подключение библиотек
target_link_libraries(tor PRIVATE 
    Boost::boost 
    Boost::system 
    SQLite::SQLite3
    ${READLINE_LIB} 
    ${HISTORY_LIB}
    ${NCURSES_LIB}
)