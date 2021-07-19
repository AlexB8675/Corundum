#include <corundum/core/command_buffer.hpp>
#include <corundum/core/swapchain.hpp>
#include <corundum/core/context.hpp>
#include <corundum/core/queue.hpp>

#include <thread>

namespace crd::core {
    crd_nodiscard crd_module Queue* make_queue(const Context& context, QueueFamily family) noexcept {
        auto* queue = new Queue();
        queue->family = family.family;
        VkCommandPoolCreateInfo command_pool_info;
        command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        command_pool_info.pNext = nullptr;
        command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        command_pool_info.queueFamilyIndex = family.family;
        crd_vulkan_check(vkCreateCommandPool(context.device, &command_pool_info, nullptr, &queue->pool));
        vkGetDeviceQueue(context.device, queue->family, family.index, &queue->handle);
        return queue;
    }

    crd_module void destroy_queue(const Context& context, Queue*& queue) noexcept {
        vkDestroyCommandPool(context.device, queue->pool, nullptr);
        delete queue;
        queue = nullptr;
    }

    crd_module void Queue::submit(const CommandBuffer& commands,
                                  VkPipelineStageFlags stage,
                                  VkSemaphore wait,
                                  VkSemaphore signal,
                                  VkFence fence) noexcept {
        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        if (wait) {
            submit_info.waitSemaphoreCount = 1;
            submit_info.pWaitSemaphores = &wait;
            submit_info.pWaitDstStageMask = &stage;
        }
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &commands.handle;
        if (signal) {
            submit_info.signalSemaphoreCount = 1;
            submit_info.pSignalSemaphores = &signal;
        }

        std::lock_guard<std::mutex> guard(lock);
        crd_vulkan_check(vkQueueSubmit(handle, 1, &submit_info, fence));
    }

    crd_module void Queue::present(const Swapchain& swapchain, std::uint32_t image, VkSemaphore wait) noexcept {
        VkResult present_result;
        VkPresentInfoKHR present_info = {};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        if (wait) {
            present_info.waitSemaphoreCount = 1;
            present_info.pWaitSemaphores = &wait;
        }
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &swapchain.handle;
        present_info.pImageIndices = &image;
        present_info.pResults = &present_result;

        std::lock_guard<std::mutex> guard(lock);
        crd_vulkan_check(vkQueuePresentKHR(handle, &present_info));
        crd_vulkan_check(present_result);
    }

    crd_module void Queue::wait_idle() noexcept {
        std::lock_guard<std::mutex> guard(lock);
        crd_vulkan_check(vkQueueWaitIdle(handle));
    }
} // namespace crd::core
