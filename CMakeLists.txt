cmake_minimum_required(VERSION 3.19)
project(corundum)

find_package(Vulkan REQUIRED)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_VULKAN_STATIC ON CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(ext/glfw)

set(SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory(ext/SPIRV-Cross)

set(FTL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FTL_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(FTL_CPP_17 ON CACHE BOOL "" FORCE)
add_subdirectory(ext/FiberTaskingLib)

set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_3DS_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_PLY_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(ext/assimp)

if(MSVC)
    target_compile_options(assimp PUBLIC /w /EHsc)
endif()

add_subdirectory(ext/glm)

set(CMAKE_CXX_STANDARD 20)
add_library(corundum STATIC
    include/corundum/core/async.hpp
    include/corundum/core/buffer.hpp
    include/corundum/core/clear.hpp
    include/corundum/core/command_buffer.hpp
    include/corundum/core/constants.hpp
    include/corundum/core/context.hpp
    include/corundum/core/descriptor_set.hpp
    include/corundum/core/expected.hpp
    include/corundum/core/image.hpp
    include/corundum/core/pipeline.hpp
    include/corundum/core/queue.hpp
    include/corundum/core/render_pass.hpp
    include/corundum/core/renderer.hpp
    include/corundum/core/static_buffer.hpp
    include/corundum/core/static_mesh.hpp
    include/corundum/core/static_model.hpp
    include/corundum/core/static_texture.hpp
    include/corundum/core/swapchain.hpp
    include/corundum/core/utilities.hpp

    include/corundum/detail/file_view.hpp
    include/corundum/detail/forward.hpp
    include/corundum/detail/hash.hpp
    include/corundum/detail/logger.hpp
    include/corundum/detail/macros.hpp

    include/corundum/wm/window.hpp

    src/core/async.cpp
    src/core/buffer.cpp
    src/core/clear.cpp
    src/core/command_buffer.cpp
    src/core/context.cpp
    src/core/descriptor_set.cpp
    src/core/image.cpp
    src/core/pipeline.cpp
    src/core/queue.cpp
    src/core/render_pass.cpp
    src/core/renderer.cpp
    src/core/static_buffer.cpp
    src/core/static_mesh.cpp
    src/core/static_model.cpp
    src/core/static_texture.cpp
    src/core/stb_image.cpp
    src/core/swapchain.cpp
    src/core/vma.cpp

    src/detail/file_view.cpp

    src/wm/window.cpp)

target_compile_definitions(corundum PUBLIC
    $<$<CONFIG:Debug>:crd_debug>
    crd_debug_logging
    crd_debug_benchmark
    crd_log_level=2

    $<$<BOOL:${WIN32}>:
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX>

    SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS

    VK_ENABLE_BETA_EXTENSIONS

    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_RADIANS)

target_compile_options(corundum PUBLIC
    $<$<NOT:$<BOOL:${WIN32}>>:-march=native -fvisibility=hidden>)

target_include_directories(corundum PUBLIC
    include
    ext/stb
    ext/vma/include)

target_link_libraries(corundum PUBLIC
    glm
    ftl
    glfw
    assimp
    Vulkan::Vulkan
    spirv-cross-glsl
    $<$<BOOL:${UNIX}>:pthread>)

add_library(tests-common INTERFACE tests/common.hpp)

target_include_directories(tests-common INTERFACE tests)
target_link_libraries(tests-common INTERFACE corundum)

add_executable(test-csm tests/test_csm.cpp)
add_executable(test-lights tests/test_lights.cpp)
add_executable(test-shadows tests/test_shadows.cpp)
add_executable(test-sponza tests/test_sponza.cpp)

target_link_libraries(test-csm PRIVATE tests-common)
target_link_libraries(test-lights PRIVATE tests-common)
target_link_libraries(test-shadows PRIVATE tests-common)
target_link_libraries(test-sponza PRIVATE tests-common)
