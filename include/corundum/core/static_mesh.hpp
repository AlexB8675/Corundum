#pragma once

#include <corundum/core/static_buffer.hpp>

#include <corundum/util/forward.hpp>
#include <corundum/util/macros.hpp>

#include <variant>
#include <cstdint>
#include <vector>
#include <future>

namespace crd::core {
    struct StaticMesh {
        struct CreateInfo {
            std::vector<float> geometry;
            std::vector<std::uint32_t> indices;
        };
        StaticBuffer geometry;
        StaticBuffer indices;
    };

    template <>
    struct Task<StaticMesh> {
       std::variant<std::future<StaticMesh>, StaticMesh> storage;

        crd_nodiscard crd_module bool        is_ready() noexcept;
        crd_nodiscard crd_module StaticMesh& get() noexcept;
    };

    crd_nodiscard crd_module Task<StaticMesh> request_static_mesh(const Context&, StaticMesh::CreateInfo&&) noexcept;
                  crd_module void             destroy_static_mesh(const Context&, StaticMesh&) noexcept;
} // namespace crd::core
