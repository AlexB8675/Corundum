#pragma once

#include <corundum/core/acceleration_structure.hpp>
#include <corundum/core/static_texture.hpp>
#include <corundum/core/static_mesh.hpp>
#include <corundum/core/async.hpp>

#include <corundum/detail/forward.hpp>
#include <corundum/detail/macros.hpp>

#include <vulkan/vulkan.h>

#include <vector>
#include <string>

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
        const Context* context;
        std::vector<TexturedMesh> submeshes;

        crd_module void destroy() noexcept;
    };

    crd_nodiscard crd_module Async<StaticModel> request_static_model(Renderer&, std::string&&) noexcept;
} // namespace crd
