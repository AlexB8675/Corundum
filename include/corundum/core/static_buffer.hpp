#pragma once

#include <corundum/detail/forward.hpp>
#include <corundum/detail/macros.hpp>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <cstddef>

namespace crd {
    struct StaticBuffer {
        struct CreateInfo {
            VkBufferUsageFlags flags;
            VmaMemoryUsage usage;
            std::size_t capacity;
        };
        const Context* context;
        VmaAllocation allocation;
        VmaMemoryUsage usage;
        VkBufferUsageFlags flags;
        std::size_t capacity;
        VkDeviceAddress address;
        VkBuffer handle;
        void* mapped;

        crd_module void destroy() noexcept;
    };

    crd_nodiscard crd_module StaticBuffer make_static_buffer(const Context&, StaticBuffer::CreateInfo&&) noexcept;
} // namespace crd
