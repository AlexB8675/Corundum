#pragma once

#include <corundum/util/forward.hpp>
#include <corundum/util/macros.hpp>

#include <vulkan/vulkan.h>

#include <cstdint>

namespace crd::wm {
    struct Window {
        GLFWwindow* handle;
        std::uint32_t width;
        std::uint32_t height;
    };

    crd_nodiscard crd_module Window       make_window(std::uint32_t, std::uint32_t, const char*) noexcept;
    crd_nodiscard crd_module bool         is_closed(const Window&) noexcept;
                  crd_module void         poll_events() noexcept;
    crd_nodiscard crd_module VkSurfaceKHR make_vulkan_surface(const core::Context&, const Window&) noexcept;
} // namespace crd::wm