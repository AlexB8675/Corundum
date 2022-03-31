#include <corundum/core/swapchain.hpp>
#include <corundum/core/renderer.hpp>
#include <corundum/core/context.hpp>
#include <corundum/core/image.hpp>

#include <corundum/detail/logger.hpp>

#include <corundum/wm/window.hpp>

namespace crd {
    static inline void sync_renderer(const Context& context, Renderer& renderer) noexcept {
        crd_vulkan_check(vkDeviceWaitIdle(context.device));
        renderer.frame_idx = 0;
        renderer.image_idx = 0;
        for (std::size_t i = 0; i < in_flight; ++i) {
            vkDestroySemaphore(context.device, renderer.img_ready[i], nullptr);
            vkDestroySemaphore(context.device, renderer.gfx_done[i], nullptr);
            vkDestroyFence(context.device, renderer.cmd_wait[i], nullptr);
        }

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
    }

    static inline void recreate_swapchain(const Context& context, Window& window, Swapchain& swapchain) noexcept {
        log("Vulkan", severity_warning, type_performance, "window resized, recreating swapchain");
        swapchain = make_swapchain(context, window, &swapchain);
        window.on_resize();
    }

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
        for (const auto [_, layout] : renderer.set_layout_cache) {
            vkDestroyDescriptorSetLayout(context.device, layout, nullptr);
        }
        destroy_command_buffers(context, std::move(renderer.gfx_cmds));
    }

    crd_nodiscard crd_module FrameInfo acquire_frame(const Context& context, Renderer& renderer, Window& window, Swapchain& swapchain) noexcept {
        const auto result = vkAcquireNextImageKHR(context.device, swapchain.handle, -1, renderer.img_ready[renderer.frame_idx], nullptr, &renderer.image_idx);
        crd_unlikely_if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            sync_renderer(context, renderer);
            recreate_swapchain(context, window, swapchain);
        }
        return {
            .commands = renderer.gfx_cmds[renderer.frame_idx],
            .image    = swapchain.images[renderer.image_idx],
            .index    = renderer.frame_idx,
            .wait     = renderer.img_ready[renderer.frame_idx],
            .signal   = renderer.gfx_done[renderer.frame_idx],
            .done     = renderer.cmd_wait[renderer.frame_idx],
        };
    }

    crd_module void present_frame(const Context& context, Renderer& renderer, PresentInfo&& info) noexcept {
        auto [commands, window, swapchain, waits, stages] = info;
        crd_vulkan_check(vkResetFences(context.device, 1, &renderer.cmd_wait[renderer.frame_idx]));
        waits.emplace_back(renderer.img_ready[renderer.frame_idx]);
        context.graphics->submit({
            .commands = commands,
            .stages   = stages,
            .waits    = std::move(waits),
            .signals  = { renderer.gfx_done[renderer.frame_idx] },
            .done     = renderer.cmd_wait[renderer.frame_idx]
        });
        const auto result = context.graphics->present(swapchain, renderer.image_idx, { renderer.gfx_done[renderer.frame_idx] });
        crd_unlikely_if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            sync_renderer(context, renderer);
            recreate_swapchain(context, window, swapchain);
        }
        renderer.frame_idx = (renderer.frame_idx + 1) % in_flight;
    }
} // namespace crd
