#include <corundum/core/command_buffer.hpp>
#include <corundum/core/render_pass.hpp>
#include <corundum/core/constants.hpp>
#include <corundum/core/pipeline.hpp>
#include <corundum/core/context.hpp>

#include <vector>

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

    crd_module void destroy_command_buffers(const Context& context, std::span<CommandBuffer> commands) noexcept {
        const auto pool = commands[0].pool;
        std::vector<VkCommandBuffer> handles;
        handles.reserve(commands.size());
        for (const auto& each : commands) {
            handles.emplace_back(each.handle);
        }
        vkFreeCommandBuffers(context.device, pool, handles.size(), handles.data());
    }

    crd_module void destroy_command_buffer(const Context& context, CommandBuffer command) noexcept {
        vkFreeCommandBuffers(context.device, command.pool, 1, &command.handle);
    }

    crd_module CommandBuffer& CommandBuffer::begin() noexcept {
        VkCommandBufferBeginInfo begin_info;
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.pNext = nullptr;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        begin_info.pInheritanceInfo = nullptr;
        crd_vulkan_check(vkBeginCommandBuffer(handle, &begin_info));
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::begin_render_pass(const RenderPass& render_pass, std::size_t index) noexcept {
        active_pass = &render_pass;
        const auto& clear_values = render_pass.clears;
        const auto& framebuffer = render_pass.framebuffers[index];
        
        VkRenderPassBeginInfo begin_info;
        begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        begin_info.pNext = nullptr;
        begin_info.renderPass = render_pass.handle;
        begin_info.framebuffer = framebuffer.handle;
        begin_info.renderArea.offset = {};
        begin_info.renderArea.extent = framebuffer.extent;
        begin_info.clearValueCount = clear_values.size();
        begin_info.pClearValues = clear_values.data();
        vkCmdBeginRenderPass(handle, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::set_viewport(VkViewport viewport) noexcept {
        vkCmdSetViewport(handle, 0, 1, &viewport);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::set_viewport(std::size_t index) noexcept {
        const auto extent = active_pass->framebuffers[index].extent;
        VkViewport viewport;
        viewport.x = 0;
        viewport.y = (float)extent.height;
        viewport.width = (float)extent.width;
        viewport.height = -(float)extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        return set_viewport(viewport);
    }

    crd_module CommandBuffer& CommandBuffer::set_scissor(VkRect2D scissor) noexcept {
        vkCmdSetScissor(handle, 0, 1, &scissor);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::set_scissor(std::size_t index) noexcept {
        const auto extent = active_pass->framebuffers[index].extent;
        VkRect2D scissor;
        scissor.offset = {};
        scissor.extent = extent;
        return set_scissor(scissor);
    }

    crd_module CommandBuffer& CommandBuffer::bind_pipeline(const Pipeline& pipeline) noexcept {
        vkCmdBindPipeline(handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle);
        active_pipeline = &pipeline;
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::push_constants(VkPipelineStageFlags flags, std::size_t size, const void* data) noexcept {
        vkCmdPushConstants(handle, active_pipeline->layout, flags, 0, size, data);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::draw(std::uint32_t vertices,
                                                  std::uint32_t instances,
                                                  std::uint32_t first_vertex,
                                                  std::uint32_t first_instance) noexcept {
        vkCmdDraw(handle, vertices, instances, first_vertex, first_instance);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::draw_indexed(std::uint32_t indices,
                                                          std::uint32_t instances,
                                                          std::uint32_t first_index,
                                                          std::uint32_t first_instance) noexcept {
        vkCmdDrawIndexed(handle, indices, instances, first_index, 0, first_instance);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::end_render_pass() noexcept {
        active_pipeline = nullptr;
        active_pass = nullptr;
        vkCmdEndRenderPass(handle);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::copy_image(const Image& source, const Image& dest) noexcept {
        VkImageCopy region;
        region.srcSubresource.aspectMask = source.aspect;
        region.srcSubresource.mipLevel = 0;
        region.srcSubresource.baseArrayLayer = 0;
        region.srcSubresource.layerCount = 1;
        region.srcOffset = {};
        region.dstSubresource.aspectMask = dest.aspect;
        region.dstSubresource.mipLevel = 0;
        region.dstSubresource.baseArrayLayer = 0;
        region.dstSubresource.layerCount = 1;
        region.dstOffset = {};
        region.extent = { source.width, source.height, 1 };
        vkCmdCopyImage(handle,
                       source.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       dest.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &region);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::blit_image(const ImageBlit& info) noexcept {
        const auto& source = *info.source_image;
        const auto& dest   = info.dest_image ? *info.dest_image : source;
        VkImageBlit blit = {};
        blit.srcSubresource.aspectMask = source.aspect;
        blit.srcSubresource.mipLevel = info.source_mip;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.srcOffsets[1].x = info.source_off.x ? info.source_off.x : 1;
        blit.srcOffsets[1].y = info.source_off.y ? info.source_off.y : 1;
        blit.srcOffsets[1].z = info.source_off.z ? info.source_off.z : 1;
        blit.dstSubresource.aspectMask = dest.aspect;
        blit.dstSubresource.mipLevel = info.dest_mip;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;
        blit.dstOffsets[1].x = info.dest_off.x ? info.dest_off.x : 1;
        blit.dstOffsets[1].y = info.dest_off.y ? info.dest_off.y : 1;
        blit.dstOffsets[1].z = info.dest_off.z ? info.dest_off.z : 1;
        vkCmdBlitImage(handle,
                       source.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       dest.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &blit, VK_FILTER_LINEAR);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::transfer_ownership(const ImageMemoryBarrier& info, const Queue& source, const Queue& dest) noexcept {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = info.source_access;
        barrier.dstAccessMask = info.dest_access;
        barrier.oldLayout = info.old_layout;
        barrier.newLayout = info.new_layout;
        barrier.srcQueueFamilyIndex = source.family;
        barrier.dstQueueFamilyIndex = dest.family;
        barrier.image = info.image->handle;
        barrier.subresourceRange.aspectMask = info.image->aspect;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = info.image->mips;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(
            handle,
            info.source_stage,
            info.dest_stage,
            VK_DEPENDENCY_BY_REGION_BIT,
            0, nullptr,
            0, nullptr,
            1, &barrier);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::insert_layout_transition(const ImageMemoryBarrier& info) noexcept {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = info.source_access;
        barrier.dstAccessMask = info.dest_access;
        barrier.oldLayout = info.old_layout;
        barrier.newLayout = info.new_layout;
        barrier.srcQueueFamilyIndex = family_ignored;
        barrier.dstQueueFamilyIndex = family_ignored;
        barrier.image = info.image->handle;
        barrier.subresourceRange = {
            .aspectMask = info.image->aspect,
            .baseMipLevel = info.mip,
            .levelCount = info.levels == 0 ? info.image->mips : info.levels,
            .baseArrayLayer = 0,
            .layerCount = 1
        };
        vkCmdPipelineBarrier(
            handle,
            info.source_stage,
            info.dest_stage,
            VK_DEPENDENCY_BY_REGION_BIT,
            0, nullptr,
            0, nullptr,
            1, &barrier);
        return *this;
    }

    crd_module void CommandBuffer::end() const noexcept {
        crd_vulkan_check(vkEndCommandBuffer(handle));
    }
} // namespace crd::core
