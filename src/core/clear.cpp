#include <corundum/core/clear.hpp>

#include <algorithm>

namespace crd::core {
    crd_nodiscard crd_module constexpr VkClearValue as_vulkan(ClearValue clear) noexcept {
        VkClearValue value = {};
        switch (clear.tag) {
            case ClearValue::eColor: {
                new (&value.color) VkClearColorValue();
                const auto* first = clear.color.color;
                const auto* last  = clear.color.color + 4;
                      auto* dest  = new (&value.color.float32) float[4];
                std::copy(first, last, dest);
            } break;

            case ClearValue::eDepth: {
                new (&value.depthStencil) VkClearDepthStencilValue{
                    .depth   = clear.depth.depth,
                    .stencil = clear.depth.stencil
                };
            } break;
        }
        return value;
    }

    crd_nodiscard crd_module ClearValue make_clear_color(ClearColor color) noexcept {
        ClearValue value;
        value.tag = ClearValue::eColor;
        new (&value.color) ClearColor(color);
        return value;
    }
    crd_nodiscard crd_module ClearValue make_clear_depth(ClearDepth depth) noexcept {
        ClearValue value;
        value.tag = ClearValue::eDepth;
        new (&value.depth) ClearDepth(depth);
        return value;
    }
} // namespace crd::core
