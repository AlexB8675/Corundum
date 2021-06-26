cmake_minimum_required(VERSION 3.19)
project(Corundum)

find_package(Vulkan REQUIRED)

set(CMAKE_CXX_STANDARD 20)

add_executable(Corundum
    include/corundum/core/context.hpp
    include/corundum/util/logger.hpp
    include/corundum/util/macros.hpp

    src/core/context.cpp

    src/main.cpp)

target_compile_definitions(Corundum PUBLIC $<${WIN32}:_CRT_SECURE_NO_WARNINGS>)
target_compile_options(Corundum PUBLIC $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-march=native>)
target_include_directories(Corundum PRIVATE include)
target_link_libraries(Corundum PRIVATE Vulkan::Vulkan)