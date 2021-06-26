#pragma once

#define crd_debug_logging


#if defined(_MSVC_LANG)
    #define crd_cpp_version _MSVC_LANG
    #define crd_unreachable() __assume(false)
#else
    #define crd_cpp_version __cplusplus
    #define crd_unreachable() __builtin_unreachable()
#endif

#if crd_cpp_version >= 201703l
    #define crd_nodiscard [[nodiscard]]
#else
    #define crd_nodiscard
#endif

#if !defined(NDEBUG)
    #include <cassert>
    #define crd_debug 1
    #define crd_assert(expr, msg) assert(expr && msg)
    #define crd_vulkan_check(expr) crd_assert(expr == VK_SUCCESS, "VkResult was not VK_SUCCESS")
#else
    #define crd_debug 0
    #define crd_assert(expr, msg) \
        do {                      \
            (void)expr;           \
            (void)msg;            \
        } while (false)
    #define crd_vulkan_check(expr) crd_assert(expr, 0)
#endif

#define crd_force_assert(msg)   \
    do {                        \
        crd_assert(false, msg); \
        crd_unreachable();      \
    } while (false)