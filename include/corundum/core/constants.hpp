#pragma once

#include <vulkan/vulkan.h>

namespace crd {
    constexpr struct inverted_viewport_tag_t {} inverted_viewport;

    constexpr auto dynamic_size      = 128u;
    constexpr auto in_flight         = 2u;
    constexpr auto vertex_components = 14; // 3 + 3 + 2 + 3 + 3
    constexpr auto vertex_size       = sizeof(float[vertex_components]);
    constexpr auto external_subpass  = VK_SUBPASS_EXTERNAL;
    constexpr auto family_ignored    = VK_QUEUE_FAMILY_IGNORED;
} // namespace crd
