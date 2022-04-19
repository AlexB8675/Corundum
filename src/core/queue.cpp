
#include <corundum/core/command_buffer.hpp>
#include <corundum/core/swapchain.hpp>
#include <corundum/core/context.hpp>
#include <corundum/core/queue.hpp>

#include <Tracy.hpp>

#include <thread>

namespace crd {
    crd_nodiscard crd_module Queue* make_queue(const Context& context, QueueFamily family) noexcept {
        crd_profile_scoped();
        auto* queue = new Queue();
        queue->family = family.family;
        VkCommandPoolCreateInfo command_pool_info;
        command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        command_pool_info.pNext = nullptr;
        command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        command_pool_info.queueFamilyIndex = family.family;
        crd_vulkan_check(vkCreateCommandPool(context.device, &command_pool_info, nullptr, &queue->pool));
        command_pool_info.flags |= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        const auto threads = std::thread::hardware_concurrency();
        queue->transient.reserve(threads);
        for (int i = 0; i < threads; ++i) {
            crd_vulkan_check(vkCreateCommandPool(context.device, &command_pool_info, nullptr, &queue->transient.emplace_back()));
        }
        vkGetDeviceQueue(context.device, queue->family, family.index, &queue->handle);
        return queue;
    }

    crd_module void destroy_queue(const Context& context, Queue*& queue) noexcept {
        crd_profile_scoped();
        for (const auto pool : queue->transient) {
            vkDestroyCommandPool(context.device, pool, nullptr);
        }
        vkDestroyCommandPool(context.device, queue->pool, nullptr);
        delete queue;
        queue = nullptr;
    }

    crd_module void Queue::submit(const SubmitInfo& submit) noexcept {
        crd_profile_scoped();
        VkSubmitInfo submit_info;
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pNext = nullptr;
        submit_info.waitSemaphoreCount = submit.waits.size();
        submit_info.pWaitSemaphores = submit.waits.data();
        submit_info.pWaitDstStageMask = submit.stages.data();
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &submit.commands.handle;
        submit_info.signalSemaphoreCount = submit.signals.size();
        submit_info.pSignalSemaphores = submit.signals.data();

        std::lock_guard<std::mutex> guard(lock);
        crd_vulkan_check(vkQueueSubmit(handle, 1, &submit_info, submit.done));
    }

    crd_module VkResult Queue::present(const Swapchain& swapchain, std::uint32_t image, std::vector<VkSemaphore>&& wait) noexcept {
        crd_profile_scoped();
        VkPresentInfoKHR present_info;
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.pNext = nullptr;
        present_info.waitSemaphoreCount = wait.size();
        present_info.pWaitSemaphores = wait.data();
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &swapchain.handle;
        present_info.pImageIndices = &image;
        present_info.pResults = nullptr;

        std::lock_guard<std::mutex> guard(lock);
        return vkQueuePresentKHR(handle, &present_info);
    }

    crd_module void Queue::wait_idle() noexcept {
        crd_profile_scoped();
        std::lock_guard<std::mutex> guard(lock);
        crd_vulkan_check(vkQueueWaitIdle(handle));
    }

    crd_module void wait_fence(const Context& context, VkFence fence) noexcept {
        crd_profile_scoped();
        crd_vulkan_check(vkWaitForFences(context.device, 1, &fence, true, -1));
    }

    crd_module void immediate_submit(const Context& context, const CommandBuffer& commands, QueueType type) noexcept {
        crd_profile_scoped();
        VkFenceCreateInfo fence_info;
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.pNext = nullptr;
        fence_info.flags = {};
        VkFence fence;
        crd_vulkan_check(vkCreateFence(context.device, &fence_info, nullptr, &fence));
        Queue* queue;
        switch (type) {
            case queue_type_graphics: queue = context.graphics; break;
            case queue_type_transfer: queue = context.transfer; break;
            case queue_type_compute:  queue = context.compute;  break;
        }
        queue->submit({
            .commands = commands,
            .stages = {},
            .waits = {},
            .signals = {},
            .done = fence
        });
        wait_fence(context, fence);
        vkDestroyFence(context.device, fence, nullptr);
    }
} // namespace crd
