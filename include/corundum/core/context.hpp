#pragma once

#include <corundum/core/queue.hpp>

#include <corundum/util/macros.hpp>

#include <ftl/task_scheduler.h>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <cstdint>
#include <future>
#include <chrono>

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
        ftl::TaskScheduler* scheduler;
        VkDescriptorPool descriptor_pool;
        Queue* graphics;
        Queue* transfer;
        Queue* compute;

    };

    crd_nodiscard crd_module Context make_context() noexcept;
                  crd_module void    destroy_context(Context& context) noexcept;
} // namespace crd::core
