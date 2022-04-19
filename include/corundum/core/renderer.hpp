#pragma once

#include <corundum/core/command_buffer.hpp>
#include <corundum/core/constants.hpp>

#include <corundum/detail/forward.hpp>
#include <corundum/detail/macros.hpp>

#include <vulkan/vulkan.h>

#include <unordered_map>
#include <cstdint>
#include <array>

namespace crd {
    template <typename T>
    using in_flight_array = std::array<T, in_flight>;


    struct FrameInfo {
        CommandBuffer& commands;
        const Image& image;
        std::uint32_t index;
        VkSemaphore wait;
        VkSemaphore signal;
        VkFence done;
    };

    struct PresentInfo {
        const CommandBuffer& commands;
        Window& window;
        Swapchain& swapchain;
        std::vector<VkSemaphore> waits;
        std::vector<VkPipelineStageFlags> stages;
    };

    struct SamplerInfo {
        VkFilter filter;
        VkBorderColor border_color;
        VkSamplerAddressMode address_mode;
        float anisotropy;
    };

    struct Renderer {
        const Context* context;

        std::uint32_t image_idx;
        std::uint32_t frame_idx;

        std::vector<CommandBuffer> gfx_cmds;
        in_flight_array<VkSemaphore> img_ready;
        in_flight_array<VkSemaphore> gfx_done;
        in_flight_array<VkFence> cmd_wait;

        // TODO: Move to another structure (Cache<T>)
        std::unordered_map<std::size_t, VkDescriptorSetLayout> set_layout_cache;
        std::unordered_map<std::size_t, VkSampler> sampler_cache;

        crd_nodiscard crd_module FrameInfo acquire_frame(Window&, Swapchain&) noexcept;
                      crd_module void      present_frame(PresentInfo&&) noexcept;
        crd_nodiscard crd_module VkSampler acquire_sampler(SamplerInfo&&) noexcept;
                      crd_module void      destroy() noexcept;
    };

    crd_nodiscard crd_module Renderer make_renderer(const Context&) noexcept;
} // namespace crd
