#pragma once

#if 1
    #define crd_debug_logging
#endif

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

#if crd_cpp_version >= 202002l
    #define crd_likely [[likely]]
    #define crd_unlikely [[unlikely]]
#else
    #define crd_likely
    #define crd_unlikely
#endif

#if defined(__clang__) || defined(__GNUC__)
    #define crd_likely_if(cnd) if (__builtin_expect(!!(cnd), true))
    #define crd_unlikely_if(cnd) if (__builtin_expect(!!(cnd), false))
#elif defined(_MSC_VER) && crd_cpp_version >= 202002l
    #define crd_likely_if(cnd) if (!!(cnd)) crd_likely
    #define crd_unlikely_if(cnd) if (!!(cnd)) crd_unlikely
#else
    #define crd_likely_if(cnd) if (cnd)
    #define crd_unlikely_if(cnd) if (cnd)
#endif

#if defined(crd_debug)
    #include <cassert>
    #define crd_assert(expr, msg) assert((expr) && msg)
    #define crd_vulkan_check(expr) crd_assert((expr) == VK_SUCCESS, "VkResult was not VK_SUCCESS")
#else
    #define crd_assert(expr, msg) \
        do {                      \
            (void)(expr);         \
            (void)(msg);          \
        } while (false)
    #define crd_vulkan_check(expr) crd_assert(expr, 0)
#endif

#define crd_force_assert(msg)   \
    do {                        \
        crd_assert(false, msg); \
        crd_unreachable();      \
    } while (false)

#define crd_make_hashable(T, name, ...)                                  \
    template <>                                                          \
    struct hash<T> {                                                     \
        crd_nodiscard size_t operator ()(const T& name) const noexcept { \
            return crd::util::hash(0, __VA_ARGS__);                      \
        }                                                                \
    }
