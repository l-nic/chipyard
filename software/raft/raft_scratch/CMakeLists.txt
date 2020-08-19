cmake_minimum_required(VERSION 2.8)

project(raft_scratch)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_executable(raft_scratch main.c)
target_link_libraries(raft_scratch raft)