cmake_minimum_required(VERSION 3.19)
project(Corundum)

find_package(Vulkan REQUIRED)

set(CMAKE_CXX_STANDARD 20)

add_executable(Corundum
    include/corundum/core/context.hpp

    include/corundum/util/macros.hpp

    src/main.cpp src/core/context.cpp)

target_compile_options(Corundum PUBLIC -march=native)
target_include_directories(Corundum PRIVATE include)
target_link_libraries(Corundum PRIVATE Vulkan::Vulkan)