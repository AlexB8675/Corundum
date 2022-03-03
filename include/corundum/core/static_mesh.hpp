#pragma once

#include <corundum/core/acceleration_structure.hpp>
#include <corundum/core/static_buffer.hpp>

#include <corundum/detail/forward.hpp>
#include <corundum/detail/macros.hpp>

#include <optional>
#include <cstdint>
#include <vector>
#include <future>

namespace crd {
    struct StaticMesh {
        struct CreateInfo {
            std::vector<float> geometry;
            std::vector<std::uint32_t> indices;
        };
        StaticBuffer geometry;
        StaticBuffer indices;
#if defined(crd_enable_raytracing)
        BottomLevelAS blas;
        TopLevelAS tlas;
#endif
    };

    crd_nodiscard crd_module Async<StaticMesh> request_static_mesh(const Context&, StaticMesh::CreateInfo&&) noexcept;
                  crd_module void              destroy_static_mesh(const Context&, StaticMesh&) noexcept;
} // namespace crd
