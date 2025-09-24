cmake_minimum_required(VERSION 3.20)
project(entities C CXX)

cmake_policy(SET CMP0063 NEW)

add_library(entities entities.c entities.cpp)
set_property(TARGET entities PROPERTY C_STANDARD 99)
target_include_directories(entities PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
