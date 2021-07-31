#include <corundum/core/render_pass.hpp>
#include <corundum/core/context.hpp>

#include <optional>
#include <utility>

namespace crd {
    crd_nodiscard static inline VkImageLayout deduce_reference_layout(const AttachmentInfo& attachment) noexcept {
        switch (attachment.image.format) {
            case VK_FORMAT_D16_UNORM:
            case VK_FORMAT_D32_SFLOAT:
                return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            case VK_FORMAT_D16_UNORM_S8_UINT:
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            case VK_FORMAT_S8_UINT:
                return VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
            default:
                return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }
        crd_unreachable();
    }

    crd_nodiscard crd_module RenderPass make_render_pass(const Context& context, RenderPass::CreateInfo&& info) noexcept {
        RenderPass render_pass;
        std::vector<VkAttachmentDescription> attachments;
        attachments.reserve(info.attachments.size());
        for (const auto& attachment : info.attachments) {
            const auto is_stencil = attachment.image.aspect & VK_IMAGE_ASPECT_STENCIL_BIT;
            const auto is_depth   = attachment.clear.tag == clear_value_depth;
            const auto load_op =
                attachment.clear.tag == clear_value_none ?
                    VK_ATTACHMENT_LOAD_OP_LOAD :
                    VK_ATTACHMENT_LOAD_OP_CLEAR;
            const auto store_op =
                attachment.discard ?
                    VK_ATTACHMENT_STORE_OP_DONT_CARE :
                    VK_ATTACHMENT_STORE_OP_STORE;
            const auto stencil_load =
                is_depth && is_stencil ?
                    VK_ATTACHMENT_LOAD_OP_CLEAR :
                    VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            const auto stencil_store =
                !is_stencil || attachment.discard ?
                    VK_ATTACHMENT_STORE_OP_DONT_CARE :
                    VK_ATTACHMENT_STORE_OP_STORE;
            VkAttachmentDescription description;
            description.flags = {};
            description.format = attachment.image.format;
            description.samples = attachment.image.samples;
            description.loadOp = load_op;
            description.storeOp = store_op;
            description.stencilLoadOp = stencil_load;
            description.stencilStoreOp = stencil_store;
            description.initialLayout = attachment.layout.initial;
            description.finalLayout = attachment.layout.final;
            attachments.emplace_back(description);
            render_pass.clears.emplace_back(as_vulkan(attachment.clear));
        }
        render_pass.attachments = std::move(info.attachments);
        std::vector<VkSubpassDescription> subpasses;
        subpasses.reserve(info.subpasses.size());
        struct SubpassStorage {
            std::vector<VkAttachmentReference> color;
            std::vector<VkAttachmentReference> input;
            std::optional<VkAttachmentReference> depth;
        };
        std::vector<SubpassStorage> subpass_storage;
        subpass_storage.reserve(info.subpasses.size());
        for (const auto& subpass : info.subpasses) {
            auto& storage = subpass_storage.emplace_back();
            const auto process_attachments = [&](const std::vector<std::uint32_t>& attachments, bool is_input, bool check_depth) {
                std::vector<VkAttachmentReference> result;
                for (const auto& index : attachments) {
                    const auto& attachment = render_pass.attachments[index];
                    VkAttachmentReference reference;
                    reference.attachment = index;
                    reference.layout = is_input ?
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL :
                        deduce_reference_layout(attachment);
                    if (check_depth && (attachment.image.aspect & VK_IMAGE_ASPECT_DEPTH_BIT)) {
                        storage.depth = reference;
                    } else {
                        result.emplace_back(reference);
                    }
                }
                return result;
            };
            storage.color = process_attachments(subpass.attachments, false, true);
            storage.input = process_attachments(subpass.input, true, false);

            VkSubpassDescription description;
            description.flags = {};
            description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // TODO: Don't hardcode.
            description.inputAttachmentCount = storage.input.size();
            description.pInputAttachments = storage.input.data();
            description.colorAttachmentCount = storage.color.size();
            description.pColorAttachments = storage.color.data();
            description.pResolveAttachments = nullptr; // TODO: Don't hardcode.
            description.pDepthStencilAttachment = storage.depth ? &*storage.depth : nullptr;
            description.preserveAttachmentCount = subpass.preserve.size();
            description.pPreserveAttachments = subpass.preserve.data();
            subpasses.emplace_back(description);
        }

        render_pass.stage = info.dependencies[0].source_stage;
        std::vector<VkSubpassDependency> dependencies;
        dependencies.reserve(info.dependencies.size());
        for (const auto& each : info.dependencies) {
            VkSubpassDependency dependency;
            dependency.srcSubpass = each.source_subpass;
            dependency.dstSubpass = each.dest_subpass;
            dependency.srcStageMask = each.source_stage;
            dependency.dstStageMask = each.dest_stage;
            dependency.srcAccessMask = each.source_access;
            dependency.dstAccessMask = each.dest_access;
            dependency.dependencyFlags = {};
            dependencies.emplace_back(dependency);
        }

        VkRenderPassCreateInfo render_pass_info;
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_info.pNext = nullptr;
        render_pass_info.flags = {};
        render_pass_info.attachmentCount = attachments.size();
        render_pass_info.pAttachments = attachments.data();
        render_pass_info.subpassCount = subpasses.size();
        render_pass_info.pSubpasses = subpasses.data();
        render_pass_info.dependencyCount = dependencies.size();
        render_pass_info.pDependencies = dependencies.data();
        crd_vulkan_check(vkCreateRenderPass(context.device, &render_pass_info, nullptr, &render_pass.handle));

        VkFramebufferCreateInfo framebuffer_info;
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.pNext = nullptr;
        framebuffer_info.flags = {};
        framebuffer_info.renderPass = render_pass.handle;
        framebuffer_info.layers = 1; // TODO: Don't hardcode.
        for (const auto& each : info.framebuffers) {
            auto& framebuffer = render_pass.framebuffers.emplace_back();
            std::vector<VkImageView> image_references;
            for (const auto& index : each.attachments) {
                const auto& attachment = render_pass.attachments[index];
                framebuffer_info.width = attachment.image.width;
                framebuffer_info.height = attachment.image.height;
                image_references.emplace_back(attachment.image.view);
            }
            framebuffer_info.attachmentCount = image_references.size();
            framebuffer_info.pAttachments = image_references.data();
            framebuffer.extent = {
                framebuffer_info.width,
                framebuffer_info.height
            };
            crd_vulkan_check(vkCreateFramebuffer(context.device, &framebuffer_info, nullptr, &framebuffer.handle));
        }
        return render_pass;
    }

    crd_module void destroy_render_pass(const Context& context, RenderPass& render_pass) noexcept {
        for (auto& each : render_pass.attachments) {
            if (each.owning) {
                destroy_image(context, each.image);
            }
        }
        for (const auto framebuffer : render_pass.framebuffers) {
            vkDestroyFramebuffer(context.device, framebuffer.handle, nullptr);
        }
        if (render_pass.handle) {
            vkDestroyRenderPass(context.device, render_pass.handle, nullptr);
        }
        render_pass = {};
    }

    crd_nodiscard crd_module const Image& RenderPass::image(std::size_t index) const noexcept {
        return attachments[index].image;
    }
} // namespace crd
