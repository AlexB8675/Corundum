#pragma once

#if defined(_MSVC_LANG)
    #define crd_cpp_version _MSVC_LANG
    #define crd_unreachable() __assume(false)
#else
    #define crd_cpp_version __cplusplus
    #define crd_unreachable() __builtin_unreachable()
#endif

#if defined(_WIN32)
    #define crd_module __declspec(dllexport)
#elif defined(__GNUC__)
    #define crd_module __attribute__((visibility("default")))
#else
    #define crd_module
#endif

#if crd_cpp_version >= 201703l
    #define crd_nodiscard [[nodiscard]]
#else
    #define crd_nodiscard
#endif

#if defined(crd_debug)
    #include <cassert>
    #define crd_assert(expr, msg) assert((expr) && msg)
    #define crd_vulkan_check(expr) crd_assert((expr) == VK_SUCCESS, "VkResult was not VK_SUCCESS")
    #define crd_debug_logging 1
#else
    #define crd_assert(expr, msg) \
        do {                      \
            (void)(expr);         \
            (void)(msg);          \
        } while (false)
    #define crd_vulkan_check(expr) crd_assert(expr, 0)
    #define crd_debug_logging 0
#endif

#define crd_force_assert(msg)   \
    do {                        \
        crd_assert(false, msg); \
        crd_unreachable();      \
    } while (false)