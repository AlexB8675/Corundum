
#include <corundum/core/command_buffer.hpp>
#include <corundum/core/swapchain.hpp>
#include <corundum/core/context.hpp>
#include <corundum/core/queue.hpp>

#include <thread>

namespace crd {
    crd_nodiscard crd_module Queue* make_queue(const Context& context, QueueFamily family) noexcept {
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
        for (const auto pool : queue->transient) {
            vkDestroyCommandPool(context.device, pool, nullptr);
        }
        vkDestroyCommandPool(context.device, queue->pool, nullptr);
        delete queue;
        queue = nullptr;
    }

    crd_module void Queue::submit(const CommandBuffer& commands,
                                  VkPipelineStageFlags stage,
                                  std::vector<VkSemaphore> wait,
                                  std::vector<VkSemaphore> signal,
                                  VkFence fence) noexcept {
        VkSubmitInfo submit_info;
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pNext = nullptr;
        submit_info.waitSemaphoreCount = wait.size();
        submit_info.pWaitSemaphores = wait.data();
        submit_info.pWaitDstStageMask = &stage;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &commands.handle;
        submit_info.signalSemaphoreCount = signal.size();
        submit_info.pSignalSemaphores = signal.data();

        std::lock_guard<std::mutex> guard(lock);
        crd_vulkan_check(vkQueueSubmit(handle, 1, &submit_info, fence));
    }

    crd_module VkResult Queue::present(const Swapchain& swapchain, std::uint32_t image, std::vector<VkSemaphore> wait) noexcept {
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
        std::lock_guard<std::mutex> guard(lock);
        crd_vulkan_check(vkQueueWaitIdle(handle));
    }
} // namespace crd
