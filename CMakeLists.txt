cmake_minimum_required(VERSION 3.9)
project(cplusfield)

set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_FLAGS -pthread)

add_executable(ifmo-os-hw hw1/cat.c)