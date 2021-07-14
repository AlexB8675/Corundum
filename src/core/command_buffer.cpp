#include <corundum/core/command_buffer.hpp>
#include <corundum/core/context.hpp>

namespace crd::core {
    crd_nodiscard crd_module std::vector<CommandBuffer> make_command_buffers(const Context& context, CommandBuffer::CreateInfo&& info) noexcept {
        VkCommandBufferAllocateInfo allocate_info;
        allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate_info.pNext = nullptr;
        allocate_info.commandPool = info.pool;
        allocate_info.level = info.level;
        allocate_info.commandBufferCount = info.count;
        std::vector<VkCommandBuffer> handles(info.count);
        crd_vulkan_check(vkAllocateCommandBuffers(context.device, &allocate_info, handles.data()));

        std::vector<CommandBuffer> command_buffers(info.count);
        for (std::size_t i = 0; const auto handle : handles) {
            command_buffers[i].handle = handle;
            command_buffers[i].active_pass = nullptr;
            command_buffers[i].active_pipeline = nullptr;
            command_buffers[i].pool = info.pool;
            ++i;
        }
        return command_buffers;
    }

    crd_nodiscard crd_module CommandBuffer make_command_buffer(const Context& context, CommandBuffer::CreateInfo&& info) noexcept {
        CommandBuffer command_buffer;
        command_buffer.active_pass = nullptr;
        command_buffer.active_pipeline = nullptr;
        command_buffer.pool = info.pool;

        VkCommandBufferAllocateInfo allocate_info;
        allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate_info.pNext = nullptr;
        allocate_info.commandPool = info.pool;
        allocate_info.level = info.level;
        allocate_info.commandBufferCount = 1;
        crd_vulkan_check(vkAllocateCommandBuffers(context.device, &allocate_info, &command_buffer.handle));
        return command_buffer;
    }
} // namespace crd::core
