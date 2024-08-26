cmake_minimum_required(VERSION 3.26)
project(micro_proj)

set(CMAKE_CXX_STANDARD 20)

find_package(fmt REQUIRED)
add_executable(${PROJECT_NAME} main.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE include)
target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt)

# Testing
find_package(GTest REQUIRED)
set(TEST_EXE "test_${PROJECT_NAME}")
add_executable(${TEST_EXE} test.cpp)
target_include_directories(${TEST_EXE} PRIVATE ${GTEST_INCLUDE_DIRS} include)
target_link_libraries(${TEST_EXE} PRIVATE GTest::Main fmt::fmt)

add_executable(recv recv.cpp)
target_include_directories(recv PRIVATE include)

add_executable(vis vis.cpp)
target_include_directories(vis PRIVATE include)