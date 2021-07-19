cmake_minimum_required(VERSION 3.19)
project(Corundum)

find_package(Vulkan REQUIRED)

set(CMAKE_CXX_STANDARD 20)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_VULKAN_STATIC ON CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(ext/glfw)

set(SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory(ext/SPIRV-Cross)

add_executable(Corundum
    include/corundum/core/clear.hpp
    include/corundum/core/command_buffer.hpp
    include/corundum/core/constants.hpp
    include/corundum/core/context.hpp
    include/corundum/core/image.hpp
    include/corundum/core/pipeline.hpp
    include/corundum/core/queue.hpp
    include/corundum/core/render_pass.hpp
    include/corundum/core/renderer.hpp
    include/corundum/core/swapchain.hpp

    include/corundum/util/file_view.hpp
    include/corundum/util/forward.hpp
    include/corundum/util/logger.hpp
    include/corundum/util/macros.hpp

    include/corundum/wm/window.hpp

    src/core/clear.cpp
    src/core/command_buffer.cpp
    src/core/context.cpp
    src/core/image.cpp
    src/core/pipeline.cpp
    src/core/queue.cpp
    src/core/render_pass.cpp
    src/core/renderer.cpp
    src/core/swapchain.cpp
    src/core/vma.cpp

    src/util/file_view.cpp

    src/wm/window.cpp

    src/main.cpp include/corundum/core/static_buffer.hpp src/core/static_buffer.cpp include/corundum/core/static_mesh.hpp src/core/static_mesh.cpp)

target_compile_definitions(Corundum PUBLIC
    $<$<CONFIG:Debug>:crd_debug>

    $<$<BOOL:${WIN32}>:
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX>

    SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS
    VK_ENABLE_BETA_EXTENSIONS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_RADIANS)

target_compile_options(Corundum PUBLIC
    $<$<NOT:$<BOOL:${WIN32}>>:-march=native -m64 -fvisibility=hidden>)

target_include_directories(Corundum PRIVATE
    include
    ext/vma/include
    ext/glfw/include)

target_link_libraries(Corundum PRIVATE
    glfw
    Vulkan::Vulkan
    spirv-cross-glsl)

file(GLOB CORUNDUM_SHADERS "data/shaders/*.spv")
file(COPY ${CORUNDUM_SHADERS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/shaders)