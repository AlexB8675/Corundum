#include <corundum/core/render_pass.hpp>

#include <optional>

namespace crd::core {
    crd_nodiscard static VkImageLayout deduce_reference_layout(const AttachmentInfo& attachment) noexcept {
        switch (attachment.image.aspect) {
            case VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT:
                return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            case VK_IMAGE_ASPECT_COLOR_BIT:
                return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            case VK_IMAGE_ASPECT_DEPTH_BIT:
                return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            case VK_IMAGE_ASPECT_STENCIL_BIT:
                return VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
        }
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }

    crd_nodiscard crd_module RenderPass make_render_pass(const Context& context, RenderPass::CreateInfo&& info) noexcept {
        RenderPass render_pass;

        std::vector<VkAttachmentDescription> attachments;
        attachments.reserve(info.attachments.size());
        std::vector<VkSubpassDescription> subpasses;
        subpasses.reserve(info.subpasses.size());
        std::vector<VkSubpassDependency> dependencies;
        dependencies.reserve(info.dependencies.size());

        for (const auto& attachment : info.attachments) {
            const auto load_op =
                attachment.clear.tag == ClearValue::eNone ?
                    VK_ATTACHMENT_LOAD_OP_LOAD :
                    VK_ATTACHMENT_LOAD_OP_CLEAR;
            const auto store_op =
                attachment.discard ?
                    VK_ATTACHMENT_STORE_OP_DONT_CARE :
                    VK_ATTACHMENT_STORE_OP_STORE;
            const auto stencil_load =
                attachment.clear.tag == ClearValue::eDepth ?
                    VK_ATTACHMENT_LOAD_OP_CLEAR :
                    VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            const auto stencil_store =
                attachment.discard || (attachment.image.aspect & VK_IMAGE_ASPECT_COLOR_BIT) ?
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
            description.initialLayout = attachment.initial;
            description.finalLayout = attachment.final;
            attachments.emplace_back(description);
            render_pass.attachments.emplace_back(attachment);
        }

        for (const auto& subpass : info.subpasses) {
            std::optional<VkAttachmentReference> depth;
            const auto process_attachments = [&](const std::vector<std::uint32_t>& attachments, bool check_depth) {
                std::vector<VkAttachmentReference> result;
                for (const auto& index : attachments) {
                    const auto& attachment = render_pass.attachments[index];
                    VkAttachmentReference reference;
                    reference.attachment = index;
                    reference.layout = deduce_reference_layout(attachment);
                    if (check_depth && (attachment.image.aspect & VK_IMAGE_ASPECT_DEPTH_BIT)) {
                        depth = reference;
                    } else {
                        result.emplace_back(reference);
                    }
                }
                return result;
            };
            const auto color    = process_attachments(subpass.attachments, true);
            const auto preserve = process_attachments(subpass.preserve, false);
            const auto input    = process_attachments(subpass.input, false);

            VkSubpassDescription description;
            description.flags = {};
            description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // TODO: Don't hardcode.
            description.inputAttachmentCount = input.size();
            description.pInputAttachments = input.data();
            description.colorAttachmentCount = color.size();
            description.pColorAttachments = color.data();
            description.pResolveAttachments = nullptr; // TODO: Don't hardcode.
            description.pDepthStencilAttachment = depth ? nullptr : &depth.value();
            description.preserveAttachmentCount = subpass.preserve.size();
            description.pPreserveAttachments = subpass.preserve.data();
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
        return render_pass;
    }
} // namespace crd::core
