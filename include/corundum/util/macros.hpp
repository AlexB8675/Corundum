#pragma once

#define crd_unreachable() __builtin_unreachable()

#if defined(_MSVC_LANG)
    #define crd_cpp_version _MSVC_LANG
#else
    #define crd_cpp_version __cplusplus
#endif

#if crd_cpp_version >= 201703l
    #define crd_nodiscard [[nodiscard]]
#else
    #define crd_nodiscard
#endif

#if !defined(NDEBUG)
    #define crd_debug 1
    #define crd_assert(expr, msg) assert(expr && msg)
#else
    #define crd_debug 0
    #define crd_assert(expr, msg) (void)expr, (void)msg
#endif

#define crd_force_assert(msg) \
    do {                      \
        assert(false && msg); \
        crd_unreachable();    \
    } while (false)
