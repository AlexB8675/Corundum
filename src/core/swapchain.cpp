#include <corundum/core/swapchain.hpp>
#include <corundum/core/context.hpp>
#include <corundum/core/image.hpp>

#include <corundum/detail/logger.hpp>

#include <corundum/wm/window.hpp>

#include <algorithm>
#include <vector>

namespace crd {
    crd_nodiscard crd_module Swapchain make_swapchain(const Context& context, Window& window, Swapchain* old) noexcept {
        Swapchain swapchain;
        if (!old) {
            detail::log("Vulkan", detail::severity_info, detail::type_general, "vulkan surface requested");
            swapchain.surface = make_vulkan_surface(context, window);
        } else {
            swapchain.surface = old->surface;
        }

        VkBool32 present_support;
        const auto family = context.families.graphics.family;
        crd_vulkan_check(vkGetPhysicalDeviceSurfaceSupportKHR(context.gpu.handle, family, swapchain.surface, &present_support));
        crd_assert(present_support, "surface or family does not support presentation");

        VkSurfaceCapabilitiesKHR capabilities;
        crd_vulkan_check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context.gpu.handle, swapchain.surface, &capabilities));

        detail::log("Vulkan", detail::severity_info, detail::type_general, "vkQueuePresentKHR: supported");
        auto image_count = capabilities.minImageCount + 1;
        crd_unlikely_if(capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
            image_count = capabilities.maxImageCount;
        }
        detail::log("Vulkan", detail::severity_info, detail::type_general, "image count: %d", image_count);

        const auto viewport = window.viewport();
        crd_unlikely_if(capabilities.currentExtent.width != -1) {
            crd_likely_if(capabilities.currentExtent.width == 0) {
                crd_vulkan_check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context.gpu.handle, swapchain.surface, &capabilities));
            }
            swapchain.width  = capabilities.currentExtent.width;
            swapchain.height = capabilities.currentExtent.height;
        } else {
            swapchain.width  = std::clamp(viewport.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            swapchain.height = std::clamp(viewport.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        }
        detail::log("Vulkan", detail::severity_info, detail::type_general, "swapchain extent: { %d, %d }", swapchain.width, swapchain.height);

        std::uint32_t format_count;
        crd_vulkan_check(vkGetPhysicalDeviceSurfaceFormatsKHR(context.gpu.handle, swapchain.surface, &format_count, nullptr));
        std::vector<VkSurfaceFormatKHR> surface_formats(format_count);
        crd_vulkan_check(vkGetPhysicalDeviceSurfaceFormatsKHR(context.gpu.handle, swapchain.surface, &format_count, surface_formats.data()));
        auto format = surface_formats[0];
        for (const auto& each : surface_formats) {
            crd_likely_if(each.format == VK_FORMAT_B8G8R8A8_SRGB && each.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
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
        swapchain_info.oldSwapchain = old ? old->handle : nullptr;
        crd_vulkan_check(vkCreateSwapchainKHR(context.device, &swapchain_info, nullptr, &swapchain.handle));

        std::vector<VkImage> images;
        crd_vulkan_check(vkGetSwapchainImagesKHR(context.device, swapchain.handle, &image_count, nullptr));
        images.resize(image_count);
        crd_vulkan_check(vkGetSwapchainImagesKHR(context.device, swapchain.handle, &image_count, images.data()));

        VkImageViewCreateInfo image_view_info;
        image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_info.pNext = nullptr;
        image_view_info.flags = {};
        image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_info.format = swapchain.format;
        image_view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_info.subresourceRange.baseMipLevel = 0;
        image_view_info.subresourceRange.levelCount = 1;
        image_view_info.subresourceRange.baseArrayLayer = 0;
        image_view_info.subresourceRange.layerCount = 1;

        for (const auto handle : images) {
            Image image;
            image.handle = handle;
            image.allocation = nullptr;
            image.samples = VK_SAMPLE_COUNT_1_BIT;
            image.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
            image.format = swapchain.format;
            image.mips = 1;
            image.width = swapchain.width;
            image.height = swapchain.height;
            image_view_info.image = handle;
            crd_vulkan_check(vkCreateImageView(context.device, &image_view_info, nullptr, &image.view));
            swapchain.images.emplace_back(image);
        }
        detail::log("Vulkan", detail::severity_info, detail::type_general, "swapchain created successfully");
        if (old) {
            destroy_swapchain(context, *old, false);
        }
        return swapchain;
    }

    crd_module void destroy_swapchain(const Context& context, Swapchain& swapchain, bool surface) noexcept {
        for (const auto& image : swapchain.images) {
            vkDestroyImageView(context.device, image.view, nullptr);
        }
        vkDestroySwapchainKHR(context.device, swapchain.handle, nullptr);
        if (surface) {
            vkDestroySurfaceKHR(context.instance, swapchain.surface, nullptr);
        }
        swapchain = {};
    }
} // namespace crd
