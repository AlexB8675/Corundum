#pragma once

#include <corundum/core/static_texture.hpp>
#include <corundum/core/static_mesh.hpp>
#include <corundum/core/async.hpp>

#include <corundum/util/forward.hpp>
#include <corundum/util/macros.hpp>

#include <vector>

namespace crd::core {
    struct TexturedMesh {
        Async<StaticMesh> mesh;
        Async<StaticTexture>* diffuse;
        Async<StaticTexture>* normal;
        Async<StaticTexture>* specular;
        std::uint32_t vertices;
        std::uint32_t indices;
    };
    using StaticModel = std::vector<TexturedMesh>;

    crd_nodiscard crd_module Async<StaticModel> request_static_model(const Context&, const char*) noexcept;
                  crd_module void               destroy_static_model(const Context&, StaticModel&) noexcept;
} // namespace crd::core
