cmake_minimum_required(VERSION 3.21)
project(DependencyFixture LANGUAGES CXX)
add_library(probe OBJECT main.cpp)
target_compile_options(probe PRIVATE /utf-8)
