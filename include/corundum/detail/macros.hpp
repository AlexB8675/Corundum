#pragma once

#if defined(_MSC_VER)
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
    #define crd_noreturn [[noreturn]]
#else
    #define crd_nodiscard
    #define crd_noreturn
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

#define crd_make_hashable(T, name, ...)                                  \
    template <>                                                          \
    struct hash<T> {                                                     \
        crd_nodiscard size_t operator ()(const T& name) const noexcept { \
            return crd::detail::hash(0, __VA_ARGS__);                    \
        }                                                                \
    }

#if defined(crd_debug_benchmark)
    #include <chrono>
    #define crd_benchmark(msg, f, ...)                                                        \
        do {                                                                                  \
            const auto start = crd_benchmark_timestamp();                                     \
            (f)(__VA_ARGS__);                                                                 \
            const auto end = crd_benchmark_timestamp();                                       \
            const auto milli = crd_benchmark_convert(std::chrono::milliseconds, start, end);  \
            detail::log("Core", detail::severity_info, detail::type_performance, msg, milli); \
        } while (false)
    #define crd_benchmark_timestamp() std::chrono::high_resolution_clock::now()
    #define crd_benchmark_convert(to, start, end) std::chrono::duration_cast<to>((end) - (start)).count()
#else
    #define crd_benchmark(msg, f, ...) (f)(__VA_ARGS__)
    #define crd_benchmark_timestamp()
    #define crd_benchmark_convert(...)
#endif
