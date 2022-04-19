#pragma once

#include <corundum/core/constants.hpp>

#include <corundum/detail/forward.hpp>
#include <corundum/detail/macros.hpp>

#include <vulkan/vulkan.h>

#include <vector>
#include <array>

namespace crd {
    template <>
    struct DescriptorSet<1> {
        struct Hashed {
            std::size_t binding;
            std::size_t descriptor;
        };
        const Context* context;
        VkDescriptorSet handle;
        std::vector<Hashed> bound;

        crd_module DescriptorSet<1>& bind(const DescriptorBinding&, VkDescriptorBufferInfo) noexcept;
        crd_module DescriptorSet<1>& bind(const DescriptorBinding&, VkDescriptorImageInfo) noexcept;
#if defined(crd_enable_raytracing)
        crd_module DescriptorSet<1>& bind(const DescriptorBinding&, const AccelerationStructure&) noexcept;
#endif
        crd_module DescriptorSet<1>& bind(const DescriptorBinding&, const std::vector<VkDescriptorImageInfo>&) noexcept;

        crd_module DescriptorSet<1>& bind(const DescriptorBinding&, std::uint32_t, VkDescriptorBufferInfo) noexcept;
        crd_module DescriptorSet<1>& bind(const DescriptorBinding&, std::uint32_t, VkDescriptorImageInfo) noexcept;
#if defined(crd_enable_raytracing)
        crd_module DescriptorSet<1>& bind(const DescriptorBinding&, std::uint32_t, const AccelerationStructure&) noexcept;
#endif
        crd_module DescriptorSet<1>& bind(const DescriptorBinding&, std::uint32_t, const std::vector<VkDescriptorImageInfo>&) noexcept;

        crd_module void              destroy() noexcept;
    };

    template <>
    struct DescriptorSet<in_flight> {
        std::array<DescriptorSet<1>, in_flight> handles;

                      crd_module DescriptorSet<in_flight>& bind(const DescriptorBinding&, VkDescriptorBufferInfo) noexcept;
                      crd_module DescriptorSet<in_flight>& bind(const DescriptorBinding&, VkDescriptorImageInfo) noexcept;
#if defined(crd_enable_raytracing)
                      crd_module DescriptorSet<in_flight>& bind(const DescriptorBinding&, const AccelerationStructure&) noexcept;
#endif
                      crd_module DescriptorSet<in_flight>& bind(const DescriptorBinding&, const std::vector<VkDescriptorImageInfo>&) noexcept;

                      crd_module DescriptorSet<in_flight>& bind(const DescriptorBinding&, std::uint32_t, VkDescriptorBufferInfo) noexcept;
                      crd_module DescriptorSet<in_flight>& bind(const DescriptorBinding&, std::uint32_t, VkDescriptorImageInfo) noexcept;
#if defined(crd_enable_raytracing)
                      crd_module DescriptorSet<in_flight>& bind(const DescriptorBinding&, std::uint32_t, const AccelerationStructure&) noexcept;
#endif
                      crd_module DescriptorSet<in_flight>& bind(const DescriptorBinding&, std::uint32_t, const std::vector<VkDescriptorImageInfo>&) noexcept;

        crd_nodiscard crd_module DescriptorSet<1>&         operator [](std::size_t) noexcept;
        crd_nodiscard crd_module const DescriptorSet<1>&   operator [](std::size_t) const noexcept;

                      crd_module void                      destroy() noexcept;
    };

    template <std::size_t N = in_flight> crd_nodiscard crd_module DescriptorSet<N> make_descriptor_set(const Context&, DescriptorSetLayout) noexcept;
} // namespace crd
