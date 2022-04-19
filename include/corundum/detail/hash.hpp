#pragma once

#include <corundum/core/acceleration_structure.hpp>
#include <corundum/core/renderer.hpp>
#include <corundum/core/pipeline.hpp>

#include <corundum/detail/forward.hpp>
#include <corundum/detail/macros.hpp>

#include <vulkan/vulkan.h>

#include <type_traits>
#include <utility>
#include <vector>
#include <span>

namespace crd::dtl {
    template <typename... Args>
    crd_nodiscard std::size_t hash(std::size_t seed, Args&&... args) noexcept {
        return ((seed ^= std::hash<std::remove_cvref_t<Args>>()(args) + 0x9e3779b9 + (seed << 6) + (seed >> 2)), ...);
    }
} // namespace crd::dtl

namespace std {
    crd_make_hashable(crd::DescriptorBinding, value, value.dynamic, value.index, value.count, value.type, value.stage);
    crd_make_hashable(crd::DescriptorSetLayout, value, value.handle, value.dynamic);
    crd_make_hashable(VkDescriptorBufferInfo, value, value.buffer, value.offset, value.range);
    crd_make_hashable(VkDescriptorImageInfo, value, value.sampler, value.imageLayout, value.imageView);
    crd_make_hashable(crd::SamplerInfo, value, value.filter, value.border_color, value.address_mode, value.anisotropy);
    crd_make_hashable(crd::StaticBuffer, value, value.handle, value.capacity);
    crd_make_hashable(crd::AccelerationStructure, value, value.handle, value.address, value.buffer);

    template <typename T>
    struct hash<vector<T>> {
        crd_nodiscard size_t operator ()(const vector<T>& value) const noexcept {
            size_t seed = 0;
            for (const auto& each : value) {
                seed = crd::dtl::hash(seed, each);
            }
            return seed;
        }
    };

    template <typename T>
    struct hash<span<T>> {
        crd_nodiscard size_t operator ()(span<T> value) const noexcept {
            size_t seed = 0;
            for (const auto& each : value) {
                seed = crd::dtl::hash(seed, each);
            }
            return seed;
        }
    };
} // namespace std
