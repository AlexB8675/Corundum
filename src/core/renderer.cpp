#include <corundum/core/swapchain.hpp>
#include <corundum/core/renderer.hpp>
#include <corundum/core/context.hpp>
#include <corundum/core/image.hpp>

namespace crd::core {
    crd_nodiscard crd_module Renderer make_renderer(const Context& context) noexcept {
        Renderer renderer;
        renderer.frame_idx = 0;
        renderer.image_idx = 0;
        renderer.gfx_cmds = make_command_buffers(context, {
            .count = in_flight,
            .pool = context.graphics->pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY
        });

        VkFenceCreateInfo fence_info;
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.pNext = nullptr;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkSemaphoreCreateInfo semaphore_info;
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphore_info.pNext = nullptr;
        semaphore_info.flags = {};

        for (std::size_t i = 0; i < in_flight; ++i) {
            crd_vulkan_check(vkCreateSemaphore(context.device, &semaphore_info, nullptr, &renderer.img_ready[i]));
            crd_vulkan_check(vkCreateSemaphore(context.device, &semaphore_info, nullptr, &renderer.gfx_done[i]));
            crd_vulkan_check(vkCreateFence(context.device, &fence_info, nullptr, &renderer.cmd_wait[i]));
        }
        return renderer;
    }

    crd_module void destroy_renderer(const Context& context, Renderer& renderer) noexcept {
        for (std::size_t i = 0; i < in_flight; ++i) {
            vkDestroySemaphore(context.device, renderer.img_ready[i], nullptr);
            vkDestroySemaphore(context.device, renderer.gfx_done[i], nullptr);
            vkDestroyFence(context.device, renderer.cmd_wait[i], nullptr);
        }
        destroy_command_buffers(context, renderer.gfx_cmds);
    }

    crd_nodiscard crd_module FrameInfo Renderer::acquire_frame(const Context& context, const Swapchain& swapchain) noexcept {
        crd_vulkan_check(vkAcquireNextImageKHR(context.device, swapchain.handle, -1, img_ready[frame_idx], nullptr, &image_idx));
        crd_vulkan_check(vkWaitForFences(context.device, 1, &cmd_wait[frame_idx], true, -1));

        return {
            .commands = gfx_cmds[frame_idx],
            .image = swapchain.images[image_idx],
            .index = frame_idx
        };
    }

    crd_module void Renderer::present_frame(const Context& context, const Swapchain& swapchain, VkPipelineStageFlags stage) noexcept {
        crd_vulkan_check(vkResetFences(context.device, 1, &cmd_wait[frame_idx]));
        context.graphics->submit(gfx_cmds[frame_idx], stage, img_ready[frame_idx], gfx_done[frame_idx], cmd_wait[frame_idx]);
        context.graphics->present(swapchain, image_idx, gfx_done[frame_idx]);

        frame_idx = (frame_idx + 1) % in_flight;
    }
} // namespace crd::core
