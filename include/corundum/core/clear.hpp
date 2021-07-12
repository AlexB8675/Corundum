#pragma once

#include <corundum/util/forward.hpp>
#include <corundum/util/macros.hpp>

#include <vulkan/vulkan.h>

#include <cstdint>

namespace crd::core {
    struct ClearColor {
        float color[4];
    };

    struct ClearDepth {
        float depth;
        std::uint32_t stencil;
    };

    struct ClearValue {
        union {
            ClearColor color;
            ClearDepth depth;
        };
        enum {
            eNone,
            eColor,
            eDepth
        } tag;
    };

    crd_nodiscard crd_module constexpr VkClearValue as_vulkan(ClearValue clear) noexcept;
    crd_nodiscard crd_module           ClearValue   make_clear_color(ClearColor) noexcept;
    crd_nodiscard crd_module           ClearValue   make_clear_depth(ClearDepth) noexcept;
} // namespace crd::core
