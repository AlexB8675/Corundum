#include <corundum/core/command_buffer.hpp>
#include <corundum/core/descriptor_set.hpp>
#include <corundum/core/static_buffer.hpp>
#include <corundum/core/static_mesh.hpp>
#include <corundum/core/render_pass.hpp>
#include <corundum/core/constants.hpp>
#include <corundum/core/dispatch.hpp>
#include <corundum/core/pipeline.hpp>
#include <corundum/core/context.hpp>
#include <corundum/core/clear.hpp>

#include <Tracy.hpp>

#include <vector>

namespace crd {
    crd_nodiscard crd_module std::vector<CommandBuffer> make_command_buffers(const Context& context, CommandBuffer::CreateInfo&& info) noexcept {
        crd_profile_scoped();
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
        crd_profile_scoped();
        CommandBuffer command_buffer;
        command_buffer.active_pass = nullptr;
        command_buffer.active_pipeline = nullptr;
        command_buffer.active_framebuffer = nullptr;
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

    crd_module void destroy_command_buffers(const Context& context, std::vector<CommandBuffer>&& commands) noexcept {
        for (auto& each : commands) {
            destroy_command_buffer(context, each);
        }
    }

    crd_module void destroy_command_buffer(const Context& context, CommandBuffer& command) noexcept {
        crd_profile_scoped();
        vkFreeCommandBuffers(context.device, command.pool, 1, &command.handle);
        command = {};
    }

    crd_module CommandBuffer& CommandBuffer::begin() noexcept {
        crd_profile_scoped();
        VkCommandBufferBeginInfo begin_info;
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.pNext = nullptr;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        begin_info.pInheritanceInfo = nullptr;
        crd_vulkan_check(vkBeginCommandBuffer(handle, &begin_info));
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::begin_render_pass(const RenderPass& render_pass, std::size_t index) noexcept {
        crd_profile_scoped();
        const auto& clear_values = render_pass.clears(index);
        const auto& framebuffer = render_pass.framebuffers[index];
        active_framebuffer = &framebuffer;
        active_pass = &render_pass;
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

    crd_module CommandBuffer& CommandBuffer::next_subpass() noexcept {
        crd_profile_scoped();
        vkCmdNextSubpass(handle, VK_SUBPASS_CONTENTS_INLINE);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::set_viewport(inverted_viewport_tag_t) noexcept {
        crd_profile_scoped();
        const auto extent = active_framebuffer->extent;
        VkViewport viewport;
        viewport.x = 0;
        viewport.y = extent.height;
        viewport.width = extent.width;
        viewport.height = -(float)extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        return set_viewport(viewport);
    }

    crd_module CommandBuffer& CommandBuffer::set_viewport(VkViewport viewport) noexcept {
        crd_profile_scoped();
        vkCmdSetViewport(handle, 0, 1, &viewport);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::set_viewport() noexcept {
        crd_profile_scoped();
        const auto extent = active_framebuffer->extent;
        VkViewport viewport;
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = extent.width;
        viewport.height = extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        return set_viewport(viewport);
    }

    crd_module CommandBuffer& CommandBuffer::set_scissor(VkRect2D scissor) noexcept {
        crd_profile_scoped();
        vkCmdSetScissor(handle, 0, 1, &scissor);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::set_scissor() noexcept {
        crd_profile_scoped();
        const auto extent = active_framebuffer->extent;
        VkRect2D scissor;
        scissor.offset = {};
        scissor.extent = extent;
        return set_scissor(scissor);
    }

    crd_module CommandBuffer& CommandBuffer::set_depth_bias(float constant, float slope) noexcept {
        crd_profile_scoped();
        vkCmdSetDepthBias(handle, constant, 0.0f, slope);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::bind_pipeline(const Pipeline& pipeline) noexcept {
        crd_profile_scoped();
        VkPipelineBindPoint bind_point;
        switch (pipeline.type) {
            case Pipeline::type_graphics:   bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;        break;
            case Pipeline::type_compute:    bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;         break;
            case Pipeline::type_raytracing: bind_point = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR; break;
        }
        vkCmdBindPipeline(handle, bind_point, pipeline.handle);
        active_pipeline = &pipeline;
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::bind_descriptor_set(std::uint32_t index, const DescriptorSet<1>& set) noexcept {
        crd_profile_scoped();
        VkPipelineBindPoint bind_point;
        switch (active_pipeline->type) {
            case Pipeline::type_graphics:   bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;        break;
            case Pipeline::type_compute:    bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;         break;
            case Pipeline::type_raytracing: bind_point = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR; break;
        }
        vkCmdBindDescriptorSets(handle, bind_point, active_pipeline->layout.pipeline, index, 1, &set.handle, 0, nullptr);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::bind_vertex_buffer(const StaticBuffer& vertex) noexcept {
        crd_profile_scoped();
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(handle, 0, 1, &vertex.handle, &offset);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::bind_index_buffer(const StaticBuffer& index) noexcept {
        crd_profile_scoped();
        vkCmdBindIndexBuffer(handle, index.handle, 0, VK_INDEX_TYPE_UINT32);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::bind_static_mesh(const StaticMesh& mesh) noexcept {
        crd_profile_scoped();
        bind_vertex_buffer(mesh.geometry);
        bind_index_buffer(mesh.indices);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::push_constants(VkShaderStageFlags flags, const void* data, std::size_t size) noexcept {
        crd_profile_scoped();
        vkCmdPushConstants(handle, active_pipeline->layout.pipeline, flags, 0, size, data);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::clear_image(const Image& image, const ClearValue& clear) noexcept {
        crd_profile_scoped();
        const auto value = as_vulkan(clear);
        VkImageSubresourceRange subresource;
        subresource.aspectMask = image.aspect;
        subresource.baseMipLevel = 0;
        subresource.levelCount = image.mips;
        subresource.baseArrayLayer = 0;
        subresource.layerCount = image.layers;
        vkCmdClearColorImage(handle, image.handle, VK_IMAGE_LAYOUT_GENERAL, &value.color, 1, &subresource);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::dispatch(std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept {
        crd_profile_scoped();
        vkCmdDispatch(handle, x, y, z);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::draw(std::uint32_t vertices,
                                                  std::uint32_t instances,
                                                  std::uint32_t first_vertex,
                                                  std::uint32_t first_instance) noexcept {
        crd_profile_scoped();
        vkCmdDraw(handle, vertices, instances, first_vertex, first_instance);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::draw_indexed(std::uint32_t indices,
                                                          std::uint32_t instances,
                                                          std::uint32_t first_index,
                                                          std::int32_t  vertex_offset,
                                                          std::uint32_t first_instance) noexcept {
        vkCmdDrawIndexed(handle, indices, instances, first_index, vertex_offset, first_instance);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::trace_rays(std::uint32_t x, std::uint32_t y) noexcept {
#if defined(crd_enable_raytracing)
        crd_profile_scoped();
        const RayTracingPipeline& pipeline = *static_cast<const RayTracingPipeline*>(active_pipeline);
        VkStridedDeviceAddressRegionKHR empty = {};
        vkCmdTraceRaysKHR(
            handle,
            &pipeline.sbt.raygen.region,
            &pipeline.sbt.raymiss.region,
            &pipeline.sbt.raychit.region,
            &empty,
            x, y, 1);
#endif
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::end_render_pass() noexcept {
        crd_profile_scoped();
        active_framebuffer = nullptr;
        active_pipeline = nullptr;
        active_pass = nullptr;
        vkCmdEndRenderPass(handle);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::build_acceleration_structure(const VkAccelerationStructureBuildGeometryInfoKHR* geometry,
                                                                          const VkAccelerationStructureBuildRangeInfoKHR* range) noexcept {
#if defined(crd_enable_raytracing)
        crd_profile_scoped();
        vkCmdBuildAccelerationStructuresKHR(handle, 1, geometry, &range);
#endif
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::copy_image(const Image& source, const Image& dest) noexcept {
        crd_profile_scoped();
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
        crd_profile_scoped();
        const auto& source = *info.source_image;
        const auto& dest = info.dest_image ? *info.dest_image : source;
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

    crd_module CommandBuffer& CommandBuffer::copy_buffer(const StaticBuffer& source, const StaticBuffer& dest) noexcept {
        crd_profile_scoped();
        VkBufferCopy region;
        region.srcOffset = 0;
        region.dstOffset = 0;
        region.size = source.capacity;
        vkCmdCopyBuffer(handle, source.handle, dest.handle, 1, &region);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::copy_buffer_to_image(const StaticBuffer& source, const Image& dest) noexcept {
        crd_profile_scoped();
        VkBufferImageCopy region;
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = dest.aspect;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { dest.width, dest.height, 1 };
        vkCmdCopyBufferToImage(handle, source.handle, dest.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::barrier(const BufferMemoryBarrier& info) noexcept {
        crd_profile_scoped();
        VkBufferMemoryBarrier barrier;
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.pNext = nullptr;
        barrier.srcAccessMask = info.source_access;
        barrier.dstAccessMask = info.dest_access;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = info.buffer->handle;
        barrier.offset = 0;
        barrier.size = info.buffer->capacity;
        vkCmdPipelineBarrier(
            handle,
            info.source_stage,
            info.dest_stage,
            VK_DEPENDENCY_BY_REGION_BIT,
            0, nullptr,
            1, &barrier,
            0, nullptr);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::barrier(const ImageMemoryBarrier& info) noexcept {
        crd_profile_scoped();
        VkImageMemoryBarrier barrier;
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.pNext = nullptr;
        barrier.srcAccessMask = info.source_access;
        barrier.dstAccessMask = info.dest_access;
        barrier.oldLayout = info.old_layout;
        barrier.newLayout = info.new_layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = info.image->handle;
        barrier.subresourceRange.aspectMask = info.image->aspect;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = info.level == 0 ? info.image->mips : info.level;
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

    crd_module CommandBuffer& CommandBuffer::barrier(VkPipelineStageFlags source_stage, VkPipelineStageFlags dest_stage) noexcept {
        crd_profile_scoped();
        vkCmdPipelineBarrier(
            handle,
            source_stage,
            dest_stage,
            VK_DEPENDENCY_BY_REGION_BIT,
            0, nullptr,
            0, nullptr,
            0, nullptr);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::memory_barrier(const MemoryBarrier& info) noexcept {
        crd_profile_scoped();
        VkMemoryBarrier barrier;
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.pNext = nullptr;
        barrier.srcAccessMask = info.source_access;
        barrier.dstAccessMask = info.dest_access;
        vkCmdPipelineBarrier(
            handle,
            info.source_stage,
            info.dest_stage,
            VK_DEPENDENCY_BY_REGION_BIT,
            1, &barrier,
            0, nullptr,
            0, nullptr);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::transfer_ownership(const BufferMemoryBarrier& info, const Queue& source, const Queue& dest) noexcept {
        crd_profile_scoped();
        VkBufferMemoryBarrier barrier;
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.pNext = nullptr;
        barrier.srcAccessMask = info.source_access;
        barrier.dstAccessMask = info.dest_access;
        barrier.srcQueueFamilyIndex = source.family;
        barrier.dstQueueFamilyIndex = dest.family;
        barrier.buffer = info.buffer->handle;
        barrier.offset = 0;
        barrier.size = info.buffer->capacity;
        vkCmdPipelineBarrier(
            handle,
            info.source_stage,
            info.dest_stage,
            VK_DEPENDENCY_BY_REGION_BIT,
            0, nullptr,
            1, &barrier,
            0, nullptr);
        return *this;
    }

    crd_module CommandBuffer& CommandBuffer::transfer_ownership(const ImageMemoryBarrier& info, const Queue& source, const Queue& dest) noexcept {
        crd_profile_scoped();
        VkImageMemoryBarrier barrier;
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.pNext = nullptr;
        barrier.srcAccessMask = info.source_access;
        barrier.dstAccessMask = info.dest_access;
        barrier.oldLayout = info.old_layout;
        barrier.newLayout = info.new_layout;
        barrier.srcQueueFamilyIndex = source.family;
        barrier.dstQueueFamilyIndex = dest.family;
        barrier.image = info.image->handle;
        barrier.subresourceRange.aspectMask = info.image->aspect;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = info.level == 0 ? info.image->mips : info.level;
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

    crd_module CommandBuffer& CommandBuffer::transition_layout(const ImageMemoryBarrier& info) noexcept {
        crd_profile_scoped();
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = info.source_access;
        barrier.dstAccessMask = info.dest_access;
        barrier.oldLayout = info.old_layout;
        barrier.newLayout = info.new_layout;
        barrier.srcQueueFamilyIndex = family_ignored;
        barrier.dstQueueFamilyIndex = family_ignored;
        barrier.image = info.image->handle;
        barrier.subresourceRange.aspectMask = info.image->aspect;
        barrier.subresourceRange.baseMipLevel = info.mip;
        barrier.subresourceRange.levelCount = info.level == 0 ? info.image->mips : info.level;
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

    crd_module CommandBuffer& CommandBuffer::end() noexcept {
        crd_profile_scoped();
        crd_vulkan_check(vkEndCommandBuffer(handle));
        return *this;
    }
} // namespace crd
