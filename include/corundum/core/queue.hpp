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

    struct Queue {
        VkQueue handle;
        VkCommandPool pool;
        std::vector<VkCommandPool> transient;
        std::uint32_t family;
        std::mutex lock;

        crd_module void     submit(const CommandBuffer&, VkPipelineStageFlags, std::vector<VkSemaphore>, std::vector<VkSemaphore>, VkFence) noexcept;
        crd_module VkResult present(const Swapchain&, std::uint32_t, std::vector<VkSemaphore>) noexcept;
        crd_module void     wait_idle() noexcept;
    };

    crd_nodiscard crd_module Queue* make_queue(const Context&, QueueFamily) noexcept;
                  crd_module void   destroy_queue(const Context&, Queue*&) noexcept;
} // namespace crd
