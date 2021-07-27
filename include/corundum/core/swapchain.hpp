#pragma once

#include <corundum/detail/forward.hpp>
#include <corundum/detail/macros.hpp>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace crd {
    struct Swapchain {
        VkSwapchainKHR handle;
        VkSurfaceKHR surface;
        std::uint32_t width;
        std::uint32_t height;
        VkFormat format;
        std::vector<Image> images;
    };

    crd_nodiscard crd_module Swapchain make_swapchain(const Context&, const Window&) noexcept;
                  crd_module void      destroy_swapchain(const Context&, Swapchain&) noexcept;
} // namespace crd
