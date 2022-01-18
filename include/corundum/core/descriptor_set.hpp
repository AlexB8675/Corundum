#pragma once

#include <corundum/core/constants.hpp>

#include <corundum/detail/forward.hpp>
#include <corundum/detail/macros.hpp>

#include <vulkan/vulkan.h>

#include <unordered_map>
#include <vector>
#include <array>

namespace crd {
    template <>
    struct DescriptorSet<1> {
        VkDescriptorSet handle;
        std::unordered_map<std::size_t, std::size_t> bound;

        crd_module DescriptorSet<1>& bind(const Context&, const DescriptorBinding&, VkDescriptorBufferInfo) noexcept;
        crd_module DescriptorSet<1>& bind(const Context&, const DescriptorBinding&, VkDescriptorImageInfo) noexcept;
        crd_module DescriptorSet<1>& bind(const Context&, const DescriptorBinding&, const std::vector<VkDescriptorImageInfo>&) noexcept;
    };

    template <>
    struct DescriptorSet<in_flight> {
        std::array<DescriptorSet<1>, in_flight> handles;

                      crd_module DescriptorSet<in_flight>& bind(const Context&, const DescriptorBinding&, VkDescriptorBufferInfo) noexcept;
                      crd_module DescriptorSet<in_flight>& bind(const Context&, const DescriptorBinding&, VkDescriptorImageInfo) noexcept;
                      crd_module DescriptorSet<in_flight>& bind(const Context&, const DescriptorBinding&, const std::vector<VkDescriptorImageInfo>&) noexcept;

        crd_nodiscard crd_module DescriptorSet<1>&         operator [](std::size_t) noexcept;
        crd_nodiscard crd_module const DescriptorSet<1>&   operator [](std::size_t) const noexcept;
    };

    template <std::size_t N = in_flight> crd_nodiscard crd_module DescriptorSet<N> make_descriptor_set(const Context&, DescriptorSetLayout) noexcept;
    template <std::size_t N = in_flight>               crd_module void             destroy_descriptor_set(const Context&, DescriptorSet<N>&) noexcept;
} // namespace crd
