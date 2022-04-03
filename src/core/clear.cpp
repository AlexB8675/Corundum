#include <corundum/core/clear.hpp>

#include <type_traits>
#include <algorithm>
#include <cstring>

namespace crd {
    crd_nodiscard crd_module VkClearValue as_vulkan(ClearValue clear) noexcept {
        VkClearValue value = {};
        switch (clear.tag) {
            case clear_value_color: {
                new (&value.color) VkClearColorValue();
                std::memcpy(value.color.float32, clear.color.color, sizeof value.color.float32);
            } break;

            case clear_value_depth: {
                new (&value.depthStencil) VkClearDepthStencilValue{
                    .depth = clear.depth.depth,
                    .stencil = clear.depth.stencil
                };
            } break;
        }
        return value;
    }

    crd_nodiscard crd_module ClearValue make_clear_color(ClearColor color) noexcept {
        ClearValue value;
        value.tag = clear_value_color;
        new (&value.color) ClearColor(color);
        return value;
    }

    crd_nodiscard crd_module ClearValue make_clear_depth(ClearDepth depth) noexcept {
        ClearValue value;
        value.tag = clear_value_depth;
        new (&value.depth) ClearDepth(depth);
        return value;
    }
} // namespace crd
