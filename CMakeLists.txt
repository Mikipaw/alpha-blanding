cmake_minimum_required(VERSION 3.20)
project(KOTE)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "-W -Wall -march=native")
set(CMAKE_CXX_RELEASE_FLAGS "${CMAKE_CXX_RELEASE_FLAGS} -O3 -ltbb")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc")

add_executable(KOTE main.cpp)
