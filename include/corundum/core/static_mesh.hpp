#pragma once

#include <corundum/core/static_buffer.hpp>

#include <corundum/util/forward.hpp>
#include <corundum/util/macros.hpp>

#include <optional>
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

    crd_nodiscard crd_module Async<StaticMesh> request_static_mesh(const Context&, StaticMesh::CreateInfo&&) noexcept;
                  crd_module void              destroy_static_mesh(const Context&, StaticMesh&) noexcept;
} // namespace crd::core
