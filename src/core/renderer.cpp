#include <corundum/core/swapchain.hpp>
#include <corundum/core/renderer.hpp>
#include <corundum/core/context.hpp>
#include <corundum/core/image.hpp>

#include <corundum/detail/hash.hpp>

#include <corundum/wm/window.hpp>

#if defined(crd_enable_profiling)
    #include <Tracy.hpp>
#endif

#include <spdlog/spdlog.h>

namespace crd {
    static inline void sync_renderer(Renderer& renderer) noexcept {
        crd_profile_scoped();
        const auto context = renderer.context;
        crd_vulkan_check(vkDeviceWaitIdle(context->device));
        renderer.frame_idx = 0;
        renderer.image_idx = 0;
        for (std::size_t i = 0; i < in_flight; ++i) {
            vkDestroySemaphore(context->device, renderer.img_ready[i], nullptr);
            vkDestroySemaphore(context->device, renderer.gfx_done[i], nullptr);
            vkDestroyFence(context->device, renderer.cmd_wait[i], nullptr);
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
            crd_vulkan_check(vkCreateSemaphore(context->device, &semaphore_info, nullptr, &renderer.img_ready[i]));
            crd_vulkan_check(vkCreateSemaphore(context->device, &semaphore_info, nullptr, &renderer.gfx_done[i]));
            crd_vulkan_check(vkCreateFence(context->device, &fence_info, nullptr, &renderer.cmd_wait[i]));
        }
    }

    static inline void recreate_swapchain(const Context& context, Window& window, Swapchain& swapchain) noexcept {
        crd_profile_scoped();
        spdlog::info("window resized, recreating swapchain");
        swapchain = make_swapchain(context, window, &swapchain);
        window.resize_callback();
    }

    crd_nodiscard crd_module Renderer make_renderer(const Context& context) noexcept {
        crd_profile_scoped();
        Renderer renderer;
        renderer.context = &context;
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

    crd_nodiscard crd_module FrameInfo Renderer::acquire_frame(Window& window, Swapchain& swapchain) noexcept {
        crd_profile_scoped();
        const auto result = vkAcquireNextImageKHR(context->device, swapchain.handle, -1, img_ready[frame_idx], nullptr, &image_idx);
        crd_unlikely_if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            sync_renderer(*this);
            recreate_swapchain(*context, window, swapchain);
        }
        return {
            .commands = gfx_cmds[frame_idx],
            .image = swapchain.images[image_idx],
            .index = frame_idx,
            .wait = img_ready[frame_idx],
            .signal = gfx_done[frame_idx],
            .done = cmd_wait[frame_idx],
        };
    }

    crd_module void Renderer::present_frame(PresentInfo&& info) noexcept {
        crd_profile_scoped();
        auto [commands, window, swapchain, waits, stages] = info;
        crd_vulkan_check(vkResetFences(context->device, 1, &cmd_wait[frame_idx]));
        waits.emplace_back(img_ready[frame_idx]);
        context->graphics->submit({
            .commands = commands,
            .stages = stages,
            .waits = std::move(waits),
            .signals = { gfx_done[frame_idx] },
            .done = cmd_wait[frame_idx]
        });
        const auto result = context->graphics->present(swapchain, image_idx, { gfx_done[frame_idx] });
        crd_unlikely_if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            sync_renderer(*this);
            recreate_swapchain(*context, window, swapchain);
        }
        frame_idx = (frame_idx + 1) % in_flight;
    }

    crd_nodiscard crd_module VkSampler Renderer::acquire_sampler(SamplerInfo&& info) noexcept {
        crd_profile_scoped();
        const auto hash = dtl::hash(0, info);
        const auto [cached, miss] = sampler_cache.try_emplace(hash);
        crd_unlikely_if(miss) {
            VkSamplerCreateInfo sampler_info;
            sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            sampler_info.pNext = nullptr;
            sampler_info.flags = {};
            sampler_info.magFilter = info.filter;
            sampler_info.minFilter = info.filter;
            sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            sampler_info.addressModeU = info.address_mode;
            sampler_info.addressModeV = info.address_mode;
            sampler_info.addressModeW = info.address_mode;
            sampler_info.mipLodBias = 0.0f;
            sampler_info.anisotropyEnable = std::fpclassify(info.anisotropy) != FP_ZERO;
            sampler_info.maxAnisotropy = info.anisotropy;
            sampler_info.compareEnable = false;
            sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
            sampler_info.minLod = 0.0f;
            sampler_info.maxLod = 8.0f;
            sampler_info.borderColor = info.border_color;
            sampler_info.unnormalizedCoordinates = false;
            crd_vulkan_check(vkCreateSampler(context->device, &sampler_info, nullptr, &cached->second));
        }
        return cached->second;
    }

    crd_module void Renderer::destroy() noexcept {
        crd_profile_scoped();
        for (std::size_t i = 0; i < in_flight; ++i) {
            vkDestroySemaphore(context->device, img_ready[i], nullptr);
            vkDestroySemaphore(context->device, gfx_done[i], nullptr);
            vkDestroyFence(context->device, cmd_wait[i], nullptr);
        }
        for (const auto [_, layout] : set_layout_cache) {
            vkDestroyDescriptorSetLayout(context->device, layout, nullptr);
        }
        for (const auto [_, sampler] : sampler_cache) {
            vkDestroySampler(context->device, sampler, nullptr);
        }
        destroy_command_buffers(*context, std::move(gfx_cmds));
    }
} // namespace crd
