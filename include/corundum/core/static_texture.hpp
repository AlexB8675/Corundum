#pragma once

#include <corundum/core/image.hpp>
#include <corundum/core/async.hpp>

#include <corundum/util/forward.hpp>
#include <corundum/util/macros.hpp>

#include <vulkan/vulkan.h>

namespace crd::core {
    enum TextureFormat {
        texture_srgb = VK_FORMAT_R8G8B8A8_SRGB,
        texture_unorm = VK_FORMAT_R8G8B8A8_UNORM
    };

    struct StaticTexture {
        Image image;
        VkSampler sampler;

        crd_nodiscard crd_module VkDescriptorImageInfo info() const noexcept;
    };
    crd_nodiscard crd_module Async<StaticTexture> request_static_texture(const Context&, const char*, TextureFormat) noexcept;
                  crd_module void                 destroy_static_texture(const Context&, StaticTexture&);
} // namespace crd::core
