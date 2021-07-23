#pragma once

#include <corundum/util/forward.hpp>
#include <corundum/util/macros.hpp>

#include <optional>
#include <future>

namespace crd::core {
    template <typename T>
    struct Async {
        std::future<T> future;
        std::optional<T> result;

        crd_nodiscard crd_module T&   operator *() noexcept;
        crd_nodiscard crd_module T*   operator ->() noexcept;
        crd_nodiscard crd_module T&   get() noexcept;
        crd_nodiscard crd_module bool is_ready() noexcept;
        crd_nodiscard crd_module bool valid() const noexcept;
    };
} // namespace crd::core
