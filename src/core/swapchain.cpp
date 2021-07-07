#include <corundum/core/swapchain.hpp>
#include <corundum/core/context.hpp>

#include <corundum/wm/window.hpp>

#include <algorithm>
#include <vector>

namespace crd::core {
    crd_nodiscard crd_module Swapchain make_swapchain(const Context& context, const wm::Window& window) noexcept {
        Swapchain swapchain;
        swapchain.surface = crd::wm::make_vulkan_surface(context, window);

        VkBool32 present_support;
        const auto family = context.families.graphics.family;
        crd_vulkan_check(vkGetPhysicalDeviceSurfaceSupportKHR(context.gpu, family, swapchain.surface, &present_support));
        crd_assert(present_support, "surface or family does not support presentation");

        VkSurfaceCapabilitiesKHR capabilities;
        crd_vulkan_check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context.gpu, swapchain.surface, &capabilities));

        auto image_count = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
            image_count = capabilities.maxImageCount;
        }

        if (capabilities.currentExtent.width != -1) {
            swapchain.width  = capabilities.currentExtent.width;
            swapchain.height = capabilities.currentExtent.height;
        } else {
            swapchain.width  = std::clamp(window.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            swapchain.height = std::clamp(window.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        }

        std::uint32_t format_count;
        crd_vulkan_check(vkGetPhysicalDeviceSurfaceFormatsKHR(context.gpu, swapchain.surface, &format_count, nullptr));
        std::vector<VkSurfaceFormatKHR> surface_formats(format_count);
        crd_vulkan_check(vkGetPhysicalDeviceSurfaceFormatsKHR(context.gpu, swapchain.surface, &format_count, surface_formats.data()));
        auto format = surface_formats[0];
        for (const auto& each : surface_formats) {
            if (each.format == VK_FORMAT_B8G8R8A8_SRGB && each.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
                format = each;
                break;
            }
        }
        swapchain.format = format.format;

        VkSwapchainCreateInfoKHR swapchain_info;
        swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchain_info.pNext = nullptr;
        swapchain_info.flags = {};
        swapchain_info.surface = swapchain.surface;
        swapchain_info.minImageCount = image_count;
        swapchain_info.imageFormat = swapchain.format;
        swapchain_info.imageColorSpace = format.colorSpace;
        swapchain_info.imageExtent = { swapchain.width, swapchain.height };
        swapchain_info.imageArrayLayers = 1;
        swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchain_info.queueFamilyIndexCount = 1;
        swapchain_info.pQueueFamilyIndices = &family;
        swapchain_info.preTransform = capabilities.currentTransform;
        swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchain_info.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        swapchain_info.clipped = true;
        swapchain_info.oldSwapchain = nullptr;
        crd_vulkan_check(vkCreateSwapchainKHR(context.device, &swapchain_info, nullptr, &swapchain.handle));

        crd_vulkan_check(vkGetSwapchainImagesKHR(context.device, swapchain.handle, &image_count, nullptr));
        swapchain.images.resize(image_count);
        crd_vulkan_check(vkGetSwapchainImagesKHR(context.device, swapchain.handle, &image_count, swapchain.images.data()));

        VkImageViewCreateInfo image_view_info;
        image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_info.pNext = nullptr;
        image_view_info.flags = {};
        image_view_info.format = swapchain.format;
        image_view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_info.subresourceRange.baseMipLevel = 0;
        image_view_info.subresourceRange.levelCount = 1;
        image_view_info.subresourceRange.baseArrayLayer = 0;
        image_view_info.subresourceRange.layerCount = 1;

        swapchain.views.reserve(image_count);
        for (const auto& image : swapchain.images) {
            image_view_info.image = image;
            crd_vulkan_check(vkCreateImageView(context.device, &image_view_info, nullptr, &swapchain.views.emplace_back()));
        }
        return swapchain;
    }
} // namespace crd::core