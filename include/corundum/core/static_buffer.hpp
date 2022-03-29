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
        VmaAllocation allocation;
        VkBufferUsageFlags flags;
        std::size_t capacity;
        VkDeviceAddress address;
        VkBuffer handle;
        void* mapped;
    };

    crd_nodiscard crd_module StaticBuffer make_static_buffer(const Context&, StaticBuffer::CreateInfo&&) noexcept;
                  crd_module void         destroy_static_buffer(const Context&, StaticBuffer&) noexcept;
} // namespace crd
