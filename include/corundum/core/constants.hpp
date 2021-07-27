#pragma once

#include <vulkan/vulkan.h>

namespace crd {
    constexpr auto dynamic_size     = 128u;
    constexpr auto in_flight        = 2u;
    constexpr auto external_subpass = VK_SUBPASS_EXTERNAL;
    constexpr auto family_ignored   = VK_QUEUE_FAMILY_IGNORED;
} // namespace crd
