#pragma once

#include <corundum/core/clear.hpp>

#include <corundum/util/forward.hpp>
#include <corundum/util/macros.hpp>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <cstdint>

namespace crd::core {
    struct Image {
        struct CreateInfo {
            std::uint32_t width;
            std::uint32_t height;
            std::uint32_t mips;
            VkFormat format;
            VkSampleCountFlagBits samples;
            VkImageUsageFlags usage;
        };

        VkImage handle;
        VkImageView view;
        VmaAllocation allocation;
        VkSampleCountFlagBits samples;
        VkImageAspectFlags aspect;
        VkFormat format;
        std::uint32_t mips;
        std::uint32_t width;
        std::uint32_t height;
    };

    crd_nodiscard crd_module Image make_image(const Context&, Image::CreateInfo&&) noexcept;
                  crd_module void  destroy_image(const Context&, Image&) noexcept;
} // namespace crd::core
