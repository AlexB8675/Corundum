#pragma once

#include <corundum/detail/forward.hpp>
#include <corundum/detail/macros.hpp>

#include <vulkan/vulkan.h>

#include <vector>
#include <mutex>

namespace crd {
    struct QueueFamily {
        std::uint32_t family;
        std::uint32_t index;
    };

    struct QueueFamilies {
        QueueFamily graphics;
        QueueFamily transfer;
        QueueFamily compute;
    };

    struct SubmitInfo {
        const CommandBuffer& commands;
        std::vector<VkPipelineStageFlags> stages;
        std::vector<VkSemaphore> waits;
        std::vector<VkSemaphore> signals;
        VkFence done;
    };

    struct Queue {
        VkQueue handle;
        VkCommandPool pool;
        std::vector<VkCommandPool> transient;
        std::uint32_t family;
        std::mutex lock;

        crd_module void     submit(const SubmitInfo&) noexcept;
        crd_module VkResult present(const Swapchain&, std::uint32_t, std::vector<VkSemaphore>) noexcept;
        crd_module void     wait_idle() noexcept;
    };

    crd_nodiscard crd_module Queue* make_queue(const Context&, QueueFamily) noexcept;
                  crd_module void   destroy_queue(const Context&, Queue*&) noexcept;

                  crd_module void   wait_fence(const Context&, VkFence) noexcept;
} // namespace crd
