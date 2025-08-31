cmake_minimum_required(VERSION 3.10)
project(proj VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS system)


add_executable(proj main.cpp)
target_link_libraries(proj PRIVATE Boost::system)
