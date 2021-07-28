#pragma once

#include <corundum/core/image.hpp>

#include <corundum/detail/forward.hpp>
#include <corundum/detail/macros.hpp>

#include <vulkan/vulkan.h>

#include <string>

namespace crd {
    enum TextureFormat {
        texture_srgb = VK_FORMAT_R8G8B8A8_SRGB,
        texture_unorm = VK_FORMAT_R8G8B8A8_UNORM
    };

    struct StaticTexture {
        Image image;
        VkSampler sampler;

        crd_nodiscard crd_module VkDescriptorImageInfo info() const noexcept;
    };
    crd_nodiscard crd_module Async<StaticTexture> request_static_texture(const Context&, std::string&&, TextureFormat) noexcept;
                  crd_module void                 destroy_static_texture(const Context&, StaticTexture&);
} // namespace crd
