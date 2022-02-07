#pragma once

#include <corundum/core/queue.hpp>

#include <corundum/detail/forward.hpp>
#include <corundum/detail/macros.hpp>

#include <ftl/task_scheduler.h>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <cstdint>
#include <future>
#include <chrono>

namespace crd {
    struct Context {
        VkInstance instance;
#if crd_debug == 1
        VkDebugUtilsMessengerEXT validation;
#endif
        struct {
            std::vector<VkExtensionProperties> extensions;
            VkPhysicalDeviceProperties properties;
            VkPhysicalDeviceFeatures features;
            VkPhysicalDevice handle;
        } gpu;
        QueueFamilies families;
        VkDevice device;
        VmaAllocator allocator;
        ftl::TaskScheduler* scheduler;
        VkDescriptorPool descriptor_pool;
        VkSampler default_sampler;
        VkSampler shadow_sampler;
        Queue* graphics;
        Queue* transfer;
        Queue* compute;
    };

    crd_nodiscard crd_module Context       make_context() noexcept;
                  crd_module void          destroy_context(Context&) noexcept;
    crd_nodiscard crd_module std::uint32_t max_bound_samplers(const Context&) noexcept;
    crd_nodiscard crd_module bool          has_extension(const Context&, const char*) noexcept;
} // namespace crd
