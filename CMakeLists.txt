cmake_minimum_required(VERSION 3.21)

project(stockbot)

set(DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(DPP_BUILD_TEST OFF)
add_subdirectory(external/DPP)

add_subdirectory(external/schwabcpp)

include_directories(
    src
    external/schwabcpp/include
    external/schwabcpp/external/spdlog/include
    external/schwabcpp/external/nlohmann_json/include
    external/DPP/include
)

file(GLOB_RECURSE SOURCES src/*.cpp)

add_executable(stockbot ${SOURCES})
target_link_libraries(stockbot PRIVATE schwabcpp dpp)
