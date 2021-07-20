#pragma once

#include <corundum/core/command_buffer.hpp>
#include <corundum/core/constants.hpp>

#include <corundum/util/forward.hpp>
#include <corundum/util/macros.hpp>

#include <vulkan/vulkan.h>

#include <unordered_map>
#include <cstdint>
#include <array>

namespace crd::core {
    struct FrameInfo {
        CommandBuffer& commands;
        const Image& image;
        std::uint32_t index;
    };

    struct Renderer {
        std::uint32_t image_idx;
        std::uint32_t frame_idx;

        std::vector<CommandBuffer> gfx_cmds;
        std::array<VkSemaphore, in_flight> img_ready;
        std::array<VkSemaphore, in_flight> gfx_done;
        std::array<VkFence, in_flight> cmd_wait;

        std::unordered_map<std::size_t, VkDescriptorSetLayout> set_layout_cache;

        crd_nodiscard crd_module FrameInfo acquire_frame(const Context&, const Swapchain&) noexcept;
                      crd_module void      present_frame(const Context&, const Swapchain&, VkPipelineStageFlags stage) noexcept;
    };

    crd_nodiscard crd_module Renderer make_renderer(const Context&) noexcept;
                  crd_module void     destroy_renderer(const Context&, Renderer&) noexcept;
} // namespace crd::core
