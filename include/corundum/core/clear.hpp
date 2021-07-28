#pragma once

#include <corundum/detail/forward.hpp>
#include <corundum/detail/macros.hpp>

#include <vulkan/vulkan.h>

#include <cstdint>

namespace crd {
    struct ClearColor {
        float color[4];
    };

    struct ClearDepth {
        float depth;
        std::uint32_t stencil;
    };

    enum ClearValueType {
        clear_value_none,
        clear_value_color,
        clear_value_depth
    };

    struct ClearValue {
        union {
            ClearColor color;
            ClearDepth depth;
        };
        ClearValueType tag;
    };

    crd_nodiscard crd_module VkClearValue as_vulkan(ClearValue clear) noexcept;
    crd_nodiscard crd_module ClearValue   make_clear_color(ClearColor) noexcept;
    crd_nodiscard crd_module ClearValue   make_clear_depth(ClearDepth) noexcept;
} // namespace crd
