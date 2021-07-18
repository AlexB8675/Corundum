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

    // TODO
    /*struct BufferMemoryBarrier {
        const StaticBuffer* buffer;
        VkPipelineStageFlags source_stage;
        VkPipelineStageFlags dest_stage;
        VkAccessFlags source_access;
        VkAccessFlags dest_access;
    };*/

    struct ImageMemoryBarrier {
        const Image* image;
        std::uint32_t mip;
        std::uint32_t levels;
        VkPipelineStageFlags source_stage;
        VkPipelineStageFlags dest_stage;
        VkAccessFlags source_access;
        VkAccessFlags dest_access;
        VkImageLayout old_layout;
        VkImageLayout new_layout;
    };

    struct Queue {
        VkQueue handle;
        VkCommandPool pool;
        std::vector<VkCommandPool> transient;
        std::uint32_t family;
        std::mutex lock;

        crd_module void submit(const CommandBuffer&, VkPipelineStageFlags, VkSemaphore, VkSemaphore, VkFence) noexcept;
        crd_module void present(const Swapchain&, std::uint32_t, VkSemaphore) noexcept;
        crd_module void wait_idle() const noexcept;
    };

    crd_nodiscard crd_module Queue* make_queue(const Context&, QueueFamily) noexcept;
                  crd_module void   destroy_queue(const Context&, Queue*&) noexcept;
} // namespace crd::core
