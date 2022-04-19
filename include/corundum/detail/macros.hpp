#pragma once

#if defined(_MSC_VER)
    #define crd_cpp_version _MSVC_LANG
    #define crd_unreachable() __assume(false)
    #define crd_signature() __FUNCSIG__
#else
    #define crd_cpp_version __cplusplus
    #define crd_unreachable() __builtin_unreachable()
    #define crd_signature() __PRETTY_FUNCTION__
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
    #define crd_noreturn [[noreturn]]
    #define crd_maybe_unused [[maybe_unused]]
#else
    #define crd_nodiscard
    #define crd_noreturn
    #define crd_maybe_unused
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
    #define crd_assert(expr, msg) assert((expr) && (msg))
    #define crd_vulkan_check(expr) crd_assert((expr) == VK_SUCCESS, "call did not return VK_SUCCESS")
#else
    #define crd_assert(expr, msg) (void)(expr)
    #define crd_vulkan_check(expr) crd_assert(expr, 0)
#endif

#define crd_force_assert(msg)   \
    do {                        \
        crd_assert(false, msg); \
        crd_unreachable();      \
    } while (false)

#define crd_panic()                 \
    do {                            \
        crd_assert(false, "panic"); \
        crd_unreachable();          \
    } while (false)

#define crd_make_hashable(T, name, ...)                                  \
    template <>                                                          \
    struct hash<T> {                                                     \
        crd_nodiscard size_t operator ()(const T& name) const noexcept { \
            return crd::dtl::hash(0, __VA_ARGS__);                    \
        }                                                                \
    }

#if defined(crd_enable_profiling)
    #define crd_profile_scoped() ZoneScoped (void)0
    #define crd_mark_frame() FrameMark (void)0
#else
    #define crd_profile_scoped()
    #define crd_mark_frame()
#endif

#define crd_load_instance_function(instance, fn) reinterpret_cast<PFN_##fn>(vkGetInstanceProcAddr(instance, #fn))
#define crd_load_device_function(device, fn) reinterpret_cast<PFN_##fn>(vkGetDeviceProcAddr(device, #fn))
