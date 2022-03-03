#pragma once

#include <corundum/core/clear.hpp>

#include <corundum/detail/forward.hpp>
#include <corundum/detail/macros.hpp>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <cstdint>

namespace crd {
    struct Image {
        struct CreateInfo {
            std::uint32_t width;
            std::uint32_t height;
            std::uint32_t mips;
            std::uint32_t layers;
            VkFormat format;
            VkImageAspectFlags aspect;
            VkSampleCountFlagBits samples;
            VkImageUsageFlags usage;
            VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        };
        VkImage handle;
        VkImageView view;
        VmaAllocation allocation;
        VkSampleCountFlagBits samples;
        VkImageAspectFlags aspect;
        VkImageUsageFlags usage;
        VkFormat format;
        std::uint32_t layers;
        std::uint32_t mips;
        std::uint32_t width;
        std::uint32_t height;

        crd_nodiscard crd_module VkDescriptorImageInfo sample(VkSampler) const noexcept;
        crd_nodiscard crd_module VkDescriptorImageInfo info(VkImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) const noexcept;
    };

    crd_nodiscard crd_module Image make_image(const Context&, Image::CreateInfo&&) noexcept;
                  crd_module void  destroy_image(const Context&, Image&) noexcept;
} // namespace crd
