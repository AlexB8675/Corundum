#include <corundum/core/context.hpp>
#include <corundum/core/queue.hpp>

namespace crd::core {
    crd_nodiscard crd_module Queue* make_queue(const Context& context, QueueFamily family) noexcept {
        auto* queue = new Queue();
        queue->family = family.family;
        VkCommandPoolCreateInfo command_pool_info;
        command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        command_pool_info.pNext = nullptr;
        command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        command_pool_info.queueFamilyIndex = family.family;
        crd_vulkan_check(vkCreateCommandPool(context.device, &command_pool_info, nullptr, &queue->main_pool));

        const auto threads = std::thread::hardware_concurrency();
        command_pool_info.flags |= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        queue->threaded_pools.reserve(threads);
        for (std::uint32_t i = 0; i < threads; ++i) {
            crd_vulkan_check(vkCreateCommandPool(context.device, &command_pool_info, nullptr, &queue->threaded_pools.emplace_back()));
        }
        vkGetDeviceQueue(context.device, queue->family, family.index, &queue->handle);
        return queue;
    }
} // namespace crd::core