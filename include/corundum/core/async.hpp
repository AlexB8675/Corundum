#pragma once

#include <corundum/detail/forward.hpp>
#include <corundum/detail/macros.hpp>

#include <optional>
#include <future>

namespace crd {
    enum class TaskTag {
        none,
        running,
        completed
    };

    template <typename T>
    struct Async {
        std::aligned_union_t<0, std::future<T>, T> storage;
        TaskTag tag = TaskTag::none;

        Async() noexcept = default;
        ~Async() noexcept;

                      crd_module void            import(std::future<T>&&) noexcept;
        crd_nodiscard crd_module T&              get() noexcept;
        crd_nodiscard crd_module T&              operator *() noexcept;
        crd_nodiscard crd_module T*              operator ->() noexcept;
        crd_nodiscard crd_module bool            is_ready() noexcept;
        crd_nodiscard            std::future<T>* future() noexcept;
        crd_nodiscard            T*              object() noexcept;
    };
} // namespace crd
