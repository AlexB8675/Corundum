#include <corundum/core/static_buffer.hpp>
#include <corundum/core/context.hpp>

#include <corundum/detail/logger.hpp>

namespace crd {
    crd_nodiscard crd_module StaticBuffer make_static_buffer(const Context& context, StaticBuffer::CreateInfo&& info) noexcept {
        VkBufferCreateInfo buffer_info;
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.pNext = nullptr;
        buffer_info.flags = {};
        buffer_info.size = info.capacity;
        buffer_info.usage = info.flags;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        buffer_info.queueFamilyIndexCount = 0;
        buffer_info.pQueueFamilyIndices = nullptr;

        VmaAllocationCreateInfo allocation_info;
        allocation_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
        allocation_info.usage = info.usage;
        allocation_info.requiredFlags = {};
        allocation_info.preferredFlags = {};
        allocation_info.memoryTypeBits = {};
        allocation_info.pool = nullptr;
        allocation_info.pUserData = nullptr;
        allocation_info.priority = 1;

        StaticBuffer buffer;
        VmaAllocationInfo extra_info;
        crd_vulkan_check(vmaCreateBuffer(
            context.allocator,
            &buffer_info,
            &allocation_info,
            &buffer.handle,
            &buffer.allocation,
            &extra_info));
        buffer.flags = info.flags;
        buffer.capacity = info.capacity;
        buffer.mapped = extra_info.pMappedData;
        detail::log("Vulkan", detail::severity_info, detail::type_general,
                  "Successfully Allocated Static Buffer, Size: %zu bytes, Address: %p",
                  buffer.capacity, buffer.mapped);
        return buffer;
    }

    crd_module void destroy_static_buffer(const Context& context, StaticBuffer& buffer) noexcept {
        vmaDestroyBuffer(context.allocator, buffer.handle, buffer.allocation);
        buffer = {};
    }
} // namespace crd