#pragma once

#include <corundum/util/forward.hpp>
#include <corundum/util/macros.hpp>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>
#include <span>

namespace crd::core {
    struct BufferMemoryBarrier {
        const StaticBuffer* buffer;
        VkPipelineStageFlags source_stage;
        VkPipelineStageFlags dest_stage;
        VkAccessFlags source_access;
        VkAccessFlags dest_access;
    };

    struct ImageMemoryBarrier {
        const Image* image;
        std::uint32_t mip;
        std::uint32_t level;
        VkPipelineStageFlags source_stage;
        VkPipelineStageFlags dest_stage;
        VkAccessFlags source_access;
        VkAccessFlags dest_access;
        VkImageLayout old_layout;
        VkImageLayout new_layout;
    };

    struct ImageBlit {
        const Image* source_image;
        const Image* dest_image;
        VkOffset3D source_off;
        VkOffset3D dest_off;
        std::uint32_t source_mip;
        std::uint32_t dest_mip;
    };

    struct CommandBuffer {
        struct CreateInfo {
            std::uint32_t count;
            VkCommandPool pool;
            VkCommandBufferLevel level;
        };
        const RenderPass* active_pass;
        const Pipeline* active_pipeline;
        VkCommandBuffer handle;
        VkCommandPool pool;

        crd_module CommandBuffer& begin() noexcept;
        crd_module CommandBuffer& begin_render_pass(const RenderPass&, std::size_t) noexcept;
        crd_module CommandBuffer& set_viewport(VkViewport) noexcept;
        crd_module CommandBuffer& set_viewport(std::size_t) noexcept;
        crd_module CommandBuffer& set_scissor(VkRect2D) noexcept;
        crd_module CommandBuffer& set_scissor(std::size_t) noexcept;
        crd_module CommandBuffer& bind_pipeline(const Pipeline&) noexcept;
        crd_module CommandBuffer& bind_descriptor_set(const DescriptorSet<1>&) noexcept;
        crd_module CommandBuffer& bind_vertex_buffer(const StaticBuffer&) noexcept;
        crd_module CommandBuffer& bind_index_buffer(const StaticBuffer&) noexcept;
        crd_module CommandBuffer& bind_static_mesh(const StaticMesh&) noexcept;
        crd_module CommandBuffer& push_constants(VkPipelineStageFlags, const void*, std::size_t) noexcept;
        crd_module CommandBuffer& draw(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) noexcept;
        crd_module CommandBuffer& draw_indexed(std::uint32_t, std::uint32_t, std::uint32_t, std::int32_t, std::uint32_t) noexcept;
        crd_module CommandBuffer& end_render_pass() noexcept;
        crd_module CommandBuffer& copy_image(const Image&, const Image&) noexcept;
        crd_module CommandBuffer& blit_image(const ImageBlit&) noexcept;
        crd_module CommandBuffer& copy_buffer(const StaticBuffer&, const StaticBuffer&) noexcept;
        crd_module CommandBuffer& copy_buffer_to_image(const StaticBuffer&, const Image&) noexcept;
        crd_module CommandBuffer& transfer_ownership(const BufferMemoryBarrier&, const Queue&, const Queue&) noexcept;
        crd_module CommandBuffer& transfer_ownership(const ImageMemoryBarrier&, const Queue&, const Queue&) noexcept;
        crd_module CommandBuffer& insert_layout_transition(const ImageMemoryBarrier&) noexcept;
        crd_module void           end() const noexcept;
    };

    crd_nodiscard crd_module std::vector<CommandBuffer> make_command_buffers(const Context&, CommandBuffer::CreateInfo&&) noexcept;
    crd_nodiscard crd_module CommandBuffer              make_command_buffer(const Context&, CommandBuffer::CreateInfo&&) noexcept;
                  crd_module void                       destroy_command_buffers(const Context&, std::span<CommandBuffer>) noexcept;
                  crd_module void                       destroy_command_buffer(const Context&, CommandBuffer) noexcept;
} // namespace crd::core
