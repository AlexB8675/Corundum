#pragma once

#include <corundum/util/macros.hpp>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <cstdint>

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

    struct Context {
        VkInstance instance;
#if crd_debug == 1
        VkDebugUtilsMessengerEXT validation;
#endif
        VkPhysicalDevice gpu;
        QueueFamilies families;
        VkDevice device;
        VmaAllocator allocator;
        VkQueue graphics;
        VkQueue transfer;
        VkQueue compute;
    };

    crd_nodiscard crd_module Context make_context() noexcept;
} // namespace crd::core
