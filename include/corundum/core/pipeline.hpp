#pragma once

#include <corundum/detail/forward.hpp>
#include <corundum/detail/macros.hpp>

#include <vulkan/vulkan.h>

#include <unordered_map>
#include <vector>
#include <string>

namespace crd {
    enum VertexAttribute {
        vertex_attribute_vec1 = sizeof(float[1]),
        vertex_attribute_vec2 = sizeof(float[2]),
        vertex_attribute_vec3 = sizeof(float[3]),
        vertex_attribute_vec4 = sizeof(float[4])
    };

    enum ColorAttachment {
        color_attachment_auto,
        color_attachment_disable_blend
    };

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
        enum {
            type_graphics,
            type_compute,
            type_raytracing,
        } type;
        VkPipeline handle;
        DescriptorLayoutBindings bindings;
        struct {
            VkPipelineLayout pipeline;
            DescriptorSetLayouts sets;
        } layout;
    };

    struct GraphicsPipeline : Pipeline {
        struct CreateInfo {
            const char* vertex;
            const char* geometry;
            const char* fragment;
            const RenderPass* render_pass;
            std::vector<VertexAttribute> attributes;
            std::vector<ColorAttachment> attachments;
            std::vector<VkDynamicState> states;
            VkCullModeFlagBits cull;
            std::uint32_t subpass;
            struct {
                bool test;
                bool write;
            } depth;
        };
    };

    struct ComputePipeline : Pipeline {
        struct CreateInfo {
            const char* compute;
            // TODO:
        };
    };

    crd_nodiscard crd_module GraphicsPipeline make_pipeline(const Context&, Renderer&, GraphicsPipeline::CreateInfo&&) noexcept;
    crd_nodiscard crd_module ComputePipeline  make_pipeline(const Context&, Renderer&, ComputePipeline::CreateInfo&&) noexcept;
                  crd_module void             destroy_pipeline(const Context&, Pipeline&) noexcept;
} // namespace crd
