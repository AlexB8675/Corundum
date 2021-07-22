#pragma once

#include <corundum/util/forward.hpp>
#include <corundum/util/macros.hpp>

#include <vulkan/vulkan.h>

#include <unordered_map>
#include <vector>
#include <string>

namespace crd::core {
    enum class VertexAttribute {
        vec1 = sizeof(float[1]),
        vec2 = sizeof(float[2]),
        vec3 = sizeof(float[3]),
        vec4 = sizeof(float[4])
    };
    constexpr auto vertex_attribute_vec1 = VertexAttribute::vec1;
    constexpr auto vertex_attribute_vec2 = VertexAttribute::vec2;
    constexpr auto vertex_attribute_vec3 = VertexAttribute::vec3;
    constexpr auto vertex_attribute_vec4 = VertexAttribute::vec4;

    struct DescriptorBinding {
        bool dynamic;
        std::uint32_t index;
        std::uint32_t count;
        VkDescriptorType type;
        VkShaderStageFlags stage;
    };

    struct DescriptorSetLayout {
        VkDescriptorSetLayout handle;
        bool dynamic;
    };

    using DescriptorSetLayouts     = std::vector<DescriptorSetLayout>;
    using DescriptorLayoutBindings = std::unordered_map<std::string, DescriptorBinding>;

    struct Pipeline {
        struct CreateInfo {
            const char* vertex;
            const char* fragment;
            const RenderPass* render_pass;
            std::vector<VertexAttribute> attributes;
            std::vector<VkDynamicState> states;
            std::uint32_t subpass;
            bool depth;
        };
        VkPipeline handle;
        VkPipelineLayout layout;
        DescriptorSetLayouts descriptors;
        DescriptorLayoutBindings bindings;
    };

    crd_nodiscard crd_module Pipeline make_pipeline(const Context&, Renderer&, Pipeline::CreateInfo&&) noexcept;
                  crd_module void     destroy_pipeline(const Context&, Pipeline&) noexcept;
} // namespace crd::core
