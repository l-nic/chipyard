cmake_minimum_required(VERSION 2.8)

project(host_driver)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_executable(host_driver main.cc)
target_link_libraries(host_driver pthread)
