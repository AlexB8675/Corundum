#pragma once

#include <corundum/core/queue.hpp>

#include <corundum/util/macros.hpp>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <cstdint>

namespace crd::core {
    struct Context {
        VkInstance instance;
#if crd_debug == 1
        VkDebugUtilsMessengerEXT validation;
#endif
        VkPhysicalDevice gpu;
        QueueFamilies families;
        VkDevice device;
        VmaAllocator allocator;
        Queue* graphics;
        Queue* transfer;
        Queue* compute;
    };

    crd_nodiscard crd_module Context make_context() noexcept;
} // namespace crd::core
