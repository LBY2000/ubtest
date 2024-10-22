cmake_minimum_required(VERSION 2.8.5 FATAL_ERROR)
project(LC_test)

#set(BOOST_ROOT "/home/byli/boost_new")
#set(Boost_INCLUDE_DIR "${BOOST_ROOT}/include")
#set(Boost_LIBRARY_DIR "${BOOST_ROOT}/lib")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_COMPILER "/usr/bin/g++")
set(CMAKE_C_FLAGS "-Wall -Wno-deprecated-declarations -Wsign-compare -O3")
#set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++14")
set(CMAKE_CXX_STANDARD 14)
set(LINKS_FLAGS "-L${Boost_LIBRARY_DIR} -Wl,-rpath,${Boost_LIBRARY_DIR} -lboost_system -lboost_coroutine  -lpthread -lcityhash -lnuma")

include_directories(${Boost_INCLUDE_DIR})
set(INCLUDE_BASE ${PROJECT_SOURCE_DIR}/include)
include_directories(${INCLUDE_BASE})


set(COMMON_SRC ${PROJECT_SOURCE_DIR}/src)

file(GLOB_RECURSE COMMON_FILE ${COMMON_SRC}/*.cpp)
add_library(lc STATIC ${COMMON_FILE})
link_libraries(lc)

add_executable(benchmark benchmark.cpp)
target_link_libraries(benchmark ${LINKS_FLAGS})

