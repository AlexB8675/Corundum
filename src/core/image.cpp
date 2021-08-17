#include <corundum/core/context.hpp>
#include <corundum/core/image.hpp>

#include <corundum/detail/logger.hpp>

#include <vulkan/vulkan.hpp>

namespace crd {
    crd_nodiscard crd_module Image make_image(const Context& context, Image::CreateInfo&& info) noexcept {
        Image image;
        image.samples = info.samples;
        image.aspect  = info.aspect;
        image.usage   = info.usage;
        image.format  = info.format;
        image.mips    = info.mips;
        image.width   = info.width;
        image.height  = info.height;

        VkImageCreateInfo image_info;
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.pNext = nullptr;
        image_info.flags = {};
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.format = image.format;
        image_info.extent = { image.width, image.height, 1 };
        image_info.mipLevels = image.mips;
        image_info.arrayLayers = 1;
        image_info.samples = info.samples;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.usage = info.usage;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.queueFamilyIndexCount = 1;
        image_info.pQueueFamilyIndices = &context.families.graphics.family;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocation_info;
        allocation_info.flags = {};
        allocation_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        allocation_info.requiredFlags = {};
        allocation_info.preferredFlags = {};
        allocation_info.memoryTypeBits = {};
        allocation_info.pool = nullptr;
        allocation_info.pUserData = nullptr;
        allocation_info.priority = 1;
        crd_vulkan_check(vmaCreateImage(
            context.allocator,
            &image_info,
            &allocation_info,
            &image.handle,
            &image.allocation,
            nullptr));

        VkMemoryRequirements memory_requirements;
        vkGetImageMemoryRequirements(context.device, image.handle, &memory_requirements);
        detail::log("Vulkan", detail::severity_info, detail::type_general,
                  "image allocated successfully: size: %llu bytes, alignment: %llu bytes",
                  memory_requirements.size, memory_requirements.alignment);
        VkImageViewCreateInfo image_view_info;
        image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_info.pNext = nullptr;
        image_view_info.flags = {};
        image_view_info.image = image.handle;
        image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_info.format = image.format;
        image_view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.subresourceRange.aspectMask = image.aspect;
        image_view_info.subresourceRange.baseMipLevel = 0;
        image_view_info.subresourceRange.levelCount = image.mips;
        image_view_info.subresourceRange.baseArrayLayer = 0;
        image_view_info.subresourceRange.layerCount = 1;
        crd_vulkan_check(vkCreateImageView(context.device, &image_view_info, nullptr, &image.view));
        return image;
    }

    crd_module void destroy_image(const Context& context, Image& image) noexcept {
        vkDestroyImageView(context.device, image.view, nullptr);
        vmaDestroyImage(context.allocator, image.handle, image.allocation);
        image = {};
    }
} // namespace crd
