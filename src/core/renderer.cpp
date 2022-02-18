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
        detail::log("Vulkan", detail::severity_warning, detail::type_performance, "window resized, recreating swapchain");
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
        for (auto& cache : renderer.compute_cache) {
            destroy_command_buffers(context, std::move(cache.commands));
            for (std::size_t i = 0; i < in_flight; ++i) {
                vkDestroySemaphore(context.device, cache.done[i], nullptr);
                vkDestroyFence(context.device, cache.wait[i], nullptr);
            }
        }
        for (const auto [_, layout] : renderer.set_layout_cache) {
            vkDestroyDescriptorSetLayout(context.device, layout, nullptr);
        }
        destroy_command_buffers(context, std::move(renderer.gfx_cmds));
    }

    crd_nodiscard crd_module FrameInfo Renderer::acquire_frame(const Context& context, Window& window, Swapchain& swapchain) noexcept {
        const auto result = vkAcquireNextImageKHR(context.device, swapchain.handle, -1, img_ready[frame_idx], nullptr, &image_idx);
        crd_unlikely_if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            sync_renderer(context, *this);
            recreate_swapchain(context, window, swapchain);
        }
        crd_vulkan_check(vkWaitForFences(context.device, 1, &cmd_wait[frame_idx], true, -1));
        return {
            .commands = gfx_cmds[frame_idx],
            .image = swapchain.images[image_idx],
            .index = frame_idx
        };
    }

    crd_module void Renderer::present_frame(const Context& context, PresentInfo&& info) noexcept {
        auto [commands, window, swapchain, waits, stage] = info;
        crd_vulkan_check(vkResetFences(context.device, 1, &cmd_wait[frame_idx]));
        waits.emplace_back(img_ready[frame_idx]);
        context.graphics->submit(commands, stage, std::move(waits), { gfx_done[frame_idx] }, cmd_wait[frame_idx]);
        const auto result = context.graphics->present(swapchain, image_idx, { gfx_done[frame_idx] });
        crd_unlikely_if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            sync_renderer(context, *this);
            recreate_swapchain(context, window, swapchain);
        }
        frame_idx = (frame_idx + 1) % in_flight;
    }

    crd_nodiscard crd_module Handle<ComputeContext> make_compute_context(const Context& context, Renderer& renderer, bool semaphore) noexcept {
        ComputeContext cmp;
        cmp.commands = make_command_buffers(context, {
            .count = in_flight,
            .pool = context.compute->pool,
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
            cmp.done[i] = nullptr;
            if (semaphore) {
                crd_vulkan_check(vkCreateSemaphore(context.device, &semaphore_info, nullptr, &cmp.done[i]));
            }
            crd_vulkan_check(vkCreateFence(context.device, &fence_info, nullptr, &cmp.wait[i]));
        }
        renderer.compute_cache.emplace_back(cmp);
        return { renderer.compute_cache.size() - 1 };
    }

    crd_nodiscard crd_module CommandBuffer& acquire_compute_commands(Renderer& renderer, Handle<ComputeContext> cmp_handle, std::uint32_t index) noexcept {
        return renderer.compute_cache[cmp_handle.index].commands[index];
    }

    crd_module void wait_compute(const Context& context, Renderer& renderer, Handle<ComputeContext> cmp_handle, std::uint32_t index) noexcept {
        crd_vulkan_check(vkWaitForFences(context.device, 1, &renderer.compute_cache[cmp_handle.index].wait[index], true, -1));
    }

    crd_module void submit_compute(const Context& context, Renderer& renderer, Handle<ComputeContext> cmp_handle, std::uint32_t index) noexcept {
        auto& compute = renderer.compute_cache[cmp_handle.index];
        crd_vulkan_check(vkResetFences(context.device, 1, &compute.wait[index]));
        context.compute->submit(compute.commands[index], {}, {}, { compute.done[index] }, compute.wait[index]);
    }
} // namespace crd
