#pragma once

#include <corundum/core/clear.hpp>
#include <corundum/core/image.hpp>

#include <corundum/detail/forward.hpp>
#include <corundum/detail/macros.hpp>

#include <vulkan/vulkan.h>

#include <vector>

namespace crd {
    struct AttachmentInfo {
        struct Layout {
            VkImageLayout initial;
            VkImageLayout final;
        };
        Image image;
        Layout layout;
        ClearValue clear;
        bool owning;
        bool discard;
    };

    struct DependencyInfo {
        std::uint32_t source_subpass;
        std::uint32_t dest_subpass;
        VkPipelineStageFlags source_stage;
        VkPipelineStageFlags dest_stage;
        VkAccessFlags source_access;
        VkAccessFlags dest_access;
    };

    struct SubpassInfo {
        std::vector<std::uint32_t> attachments;
        std::vector<std::uint32_t> preserve;
        std::vector<std::uint32_t> input;
    };

    struct ResizeAttachments {
        VkExtent2D size;
        std::uint32_t framebuffer;
        std::vector<std::uint32_t> attachments;
    };

    struct FramebufferInfo {
        std::vector<std::uint32_t> attachments;
    };

    struct Framebuffer {
        VkFramebuffer handle;
        VkExtent2D extent;
        std::vector<std::uint32_t> attachments;
    };

    struct RenderPass {
        struct CreateInfo {
            std::vector<AttachmentInfo> attachments;
            std::vector<SubpassInfo> subpasses;
            std::vector<DependencyInfo> dependencies;
            std::vector<FramebufferInfo> framebuffers;
        };
        VkRenderPass handle;
        VkPipelineStageFlags stage;
        std::vector<Framebuffer> framebuffers;
        std::vector<AttachmentInfo> attachments;

        crd_nodiscard crd_module const Image&              image(std::size_t) const noexcept;
        crd_nodiscard crd_module std::vector<VkClearValue> clears(std::size_t) const noexcept;
                      crd_module void                      resize(const Context&, ResizeAttachments&&) noexcept;
    };

    crd_nodiscard crd_module RenderPass make_render_pass(const Context&, RenderPass::CreateInfo&&) noexcept;
                  crd_module void       destroy_render_pass(const Context&, RenderPass&) noexcept;
} // namespace crd
