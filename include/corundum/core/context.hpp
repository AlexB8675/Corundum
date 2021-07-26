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
        VkPhysicalDeviceProperties gpu_properties;
        VkPhysicalDevice gpu;
        QueueFamilies families;
        VkDevice device;
        VmaAllocator allocator;
        ftl::TaskScheduler* scheduler;
        VkDescriptorPool descriptor_pool;
        VkSampler default_sampler;
        Queue* graphics;
        Queue* transfer;
        Queue* compute;

    };

    crd_nodiscard crd_module Context       make_context() noexcept;
                  crd_module void          destroy_context(Context&) noexcept;
    crd_nodiscard crd_module std::uint32_t max_bound_samplers(const Context&) noexcept;
} // namespace crd::core
