#include <corundum/core/acceleration_structure.hpp>
#include <corundum/core/static_buffer.hpp>
#include <corundum/core/utilities.hpp>
#include <corundum/core/dispatch.hpp>
#include <corundum/core/context.hpp>
#include <corundum/core/buffer.hpp>

#include <Tracy.hpp>

namespace crd {
    crd_nodiscard static inline VkDeviceAddress as_device_address(const Context& context, const AccelerationStructure& as) noexcept {
#if defined(crd_enable_raytracing)
        crd_profile_scoped();
        VkAccelerationStructureDeviceAddressInfoKHR info;
        info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        info.pNext = nullptr;
        info.accelerationStructure = as.handle;
        return vkGetAccelerationStructureDeviceAddressKHR(context.device, &info);
#else
        return {};
#endif
    }

    template <>
    crd_nodiscard crd_module VkDeviceAddress device_address(const Context& context, const StaticBuffer& buffer) noexcept {
        crd_profile_scoped();
        VkBufferDeviceAddressInfo info;
        info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        info.pNext = nullptr;
        info.buffer = buffer.handle;
        return vkGetBufferDeviceAddress(context.device, &info);
    }

    template <>
    crd_nodiscard crd_module VkDeviceAddress device_address(const Context& context, const Buffer<1>& buffer) noexcept {
        crd_profile_scoped();
        return device_address(context, buffer.handle);
    }

    template <>
    crd_nodiscard crd_module VkDeviceAddress device_address(const Context& context, const TopLevelAS& as) noexcept {
        crd_profile_scoped();
        return as_device_address(context, as);
    }

    template <>
    crd_nodiscard crd_module VkDeviceAddress device_address(const Context& context, const BottomLevelAS& as) noexcept {
        crd_profile_scoped();
        return as_device_address(context, as);
    }
} // namespace crd
