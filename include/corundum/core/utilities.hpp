#pragma once

#include <corundum/detail/forward.hpp>
#include <corundum/detail/macros.hpp>

#include <vulkan/vulkan.h>

#include <cstddef>

namespace crd {
    template <typename C>
    crd_nodiscard crd_module constexpr std::size_t size_bytes(const C& container) noexcept {
        return container.size() * sizeof(typename C::value_type);
    }

    template <typename T>
    crd_nodiscard crd_module VkDeviceAddress device_address(const Context&, const T&) noexcept;
} // namespace crd
