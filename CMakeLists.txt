cmake_minimum_required(VERSION 3.19)
project(Corundum)

find_package(Vulkan REQUIRED)

set(CMAKE_CXX_STANDARD 20)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_VULKAN_STATIC ON CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(ext/glfw)

add_executable(Corundum
    include/corundum/core/context.hpp

    include/corundum/util/logger.hpp
    include/corundum/util/macros.hpp

    include/corundum/wm/window.hpp

    src/core/context.cpp

    src/wm/window.cpp

    src/main.cpp include/corundum/util/forward.hpp include/corundum/core/swapchain.hpp src/core/swapchain.cpp)

target_compile_definitions(Corundum PUBLIC
    $<$<BOOL:${WIN32}>:
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX>

    VK_ENABLE_BETA_EXTENSIONS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_RADIANS)

target_compile_options(Corundum PUBLIC $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-march=native>)

target_include_directories(Corundum PRIVATE
    include
    ext/glfw/include)

target_link_libraries(Corundum PRIVATE
    glfw
    Vulkan::Vulkan)