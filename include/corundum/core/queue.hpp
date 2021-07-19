#pragma once

#include <corundum/util/forward.hpp>
#include <corundum/util/macros.hpp>

#include <vulkan/vulkan.h>

#include <vector>
#include <mutex>

namespace crd::core {
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

        crd_module void submit(const CommandBuffer&, VkPipelineStageFlags, VkSemaphore, VkSemaphore, VkFence) noexcept;
        crd_module void present(const Swapchain&, std::uint32_t, VkSemaphore) noexcept;
        crd_module void wait_idle() noexcept;
    };

    crd_nodiscard crd_module Queue* make_queue(const Context&, QueueFamily) noexcept;
                  crd_module void   destroy_queue(const Context&, Queue*&) noexcept;
} // namespace crd::core
