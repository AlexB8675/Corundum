#include <corundum/core/static_buffer.hpp>
#include <corundum/core/utilities.hpp>
#include <corundum/core/context.hpp>
#include <corundum/core/buffer.hpp>

namespace crd {
    template <>
    crd_nodiscard crd_module VkDeviceAddress device_address(const Context& context, const StaticBuffer& buffer) noexcept {
        VkBufferDeviceAddressInfo info;
        info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        info.pNext = nullptr;
        info.buffer = buffer.handle;
        return vkGetBufferDeviceAddress(context.device, &info);
    }

    template <>
    crd_nodiscard crd_module VkDeviceAddress device_address(const Context& context, const Buffer<1>& buffer) noexcept {
        return device_address(context, buffer.handle);
    }
} // namespace crd