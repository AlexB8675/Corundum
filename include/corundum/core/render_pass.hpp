#pragma once

#include <corundum/core/clear.hpp>
#include <corundum/core/image.hpp>

#include <corundum/util/forward.hpp>
#include <corundum/util/macros.hpp>

#include <vulkan/vulkan.h>

#include <vector>

namespace crd::core {
    struct AttachmentInfo {
        Image image;
        VkImageLayout initial;
        VkImageLayout final;
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

    struct FramebufferInfo {
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
        std::vector<VkClearValue> clears;
        std::vector<VkFramebuffer> framebuffers;
        std::vector<AttachmentInfo> attachments;
    };

    crd_nodiscard crd_module RenderPass make_render_pass(const Context&, RenderPass::CreateInfo&&) noexcept;
} // namespace crd::core
