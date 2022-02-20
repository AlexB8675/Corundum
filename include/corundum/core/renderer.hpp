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
    struct FrameInfo {
        CommandBuffer& commands;
        const Image& image;
        std::uint32_t index;
    };

    struct ComputeContext {
        std::vector<CommandBuffer> commands;
        std::array<VkSemaphore, in_flight> done;
        std::array<VkFence, in_flight> wait;
    };

    struct PresentInfo {
        const CommandBuffer& commands;
        Window& window;
        Swapchain& swapchain;
        std::vector<VkSemaphore> waits;
        VkPipelineStageFlags stage;
    };

    struct Renderer {
        std::uint32_t image_idx;
        std::uint32_t frame_idx;

        std::vector<CommandBuffer> gfx_cmds;
        std::array<VkSemaphore, in_flight> img_ready;
        std::array<VkSemaphore, in_flight> gfx_done;
        std::array<VkFence, in_flight> cmd_wait;

        // TODO: Move to another structure (Cache<T>)
        std::vector<ComputeContext> compute_cache;
        std::unordered_map<std::size_t, VkDescriptorSetLayout> set_layout_cache;

        crd_nodiscard crd_module FrameInfo acquire_frame(const Context&, Window&, Swapchain&) noexcept;
                      crd_module void      present_frame(const Context&, PresentInfo&&) noexcept;
    };

    crd_nodiscard crd_module Renderer               make_renderer(const Context&) noexcept;
                  crd_module void                   destroy_renderer(const Context&, Renderer&) noexcept;
    crd_nodiscard crd_module Handle<ComputeContext> make_compute_context(const Context&, Renderer&, bool) noexcept;
    crd_nodiscard crd_module CommandBuffer&         acquire_compute_commands(Renderer&, Handle<ComputeContext>, std::uint32_t) noexcept;
                  crd_module void                   wait_compute(const Context&, Renderer&, Handle<ComputeContext>, std::uint32_t) noexcept;
                  crd_module void                   submit_compute(const Context&, Renderer&, Handle<ComputeContext>, std::uint32_t) noexcept;
} // namespace crd
