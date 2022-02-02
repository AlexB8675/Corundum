#pragma once

#include <corundum/detail/forward.hpp>
#include <corundum/detail/macros.hpp>

#include <optional>
#include <future>

namespace crd {
    enum TaskTag {
        task_tag_none,
        task_tag_running,
        task_tag_completed
    };

    template <typename T>
    struct Async {
        std::aligned_union_t<0, std::future<T>, T> storage;
        TaskTag tag = task_tag_none;

        Async() noexcept = default;
        ~Async() noexcept;
        Async(const Async&) noexcept = delete;
        Async& operator =(const Async&) noexcept = delete;
        Async(Async&&) noexcept;
        Async& operator =(Async&&) noexcept;

                      crd_module void            import(std::future<T>&&) noexcept;
        crd_nodiscard crd_module T&              get() noexcept;
        crd_nodiscard crd_module T&              operator *() noexcept;
        crd_nodiscard crd_module T*              operator ->() noexcept;
        crd_nodiscard crd_module bool            is_ready() noexcept;
        crd_nodiscard            std::future<T>* future() noexcept;
        crd_nodiscard            T*              object() noexcept;
    };
} // namespace crd
