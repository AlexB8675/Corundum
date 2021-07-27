#pragma once

#include <corundum/core/static_texture.hpp>
#include <corundum/core/static_mesh.hpp>
#include <corundum/core/async.hpp>

#include <corundum/detail/forward.hpp>
#include <corundum/detail/macros.hpp>

#include <vulkan/vulkan.h>

#include <vector>

namespace crd {
    struct TexturedMesh {
        Async<StaticMesh> mesh;
        Async<StaticTexture>* diffuse;
        Async<StaticTexture>* normal;
        Async<StaticTexture>* specular;
        std::uint32_t vertices;
        std::uint32_t indices;
    };

    struct StaticModel {
        std::vector<TexturedMesh> submeshes;
    };

    crd_nodiscard crd_module Async<StaticModel> request_static_model(const Context&, const char*) noexcept;
                  crd_module void               destroy_static_model(const Context&, StaticModel&) noexcept;
} // namespace crd
