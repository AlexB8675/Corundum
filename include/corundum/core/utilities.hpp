#pragma once

#include <cstddef>

namespace crd {
    template <typename C>
    constexpr std::size_t size_bytes(const C& container) noexcept {
        return container.size() * sizeof(typename C::value_type);
    }
} // namespace crd
