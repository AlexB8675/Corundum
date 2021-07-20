#include <corundum/core/render_pass.hpp>
#include <corundum/core/pipeline.hpp>
#include <corundum/core/renderer.hpp>
#include <corundum/core/context.hpp>

#include <corundum/util/file_view.hpp>
#include <corundum/util/hash.hpp>

#include <spirv_glsl.hpp>
#include <spirv.hpp>

#include <numeric>
#include <cstring>

namespace crd::core {
    crd_nodiscard static inline std::vector<std::uint32_t> import_spirv(const char* path) noexcept {
        auto file = util::make_file_view(path);
        std::vector<std::uint32_t> code(file.size / sizeof(std::uint32_t));
        std::memcpy(code.data(), file.data, file.size);
        util::destroy_file_view(file);
        return code;
    }

    crd_nodiscard crd_module Pipeline make_pipeline(const Context& context, Renderer& renderer, Pipeline::CreateInfo&& info) noexcept {
        Pipeline pipeline;
        VkPipelineShaderStageCreateInfo pipeline_stages[2];
        pipeline_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_stages[0].pNext = nullptr;
        pipeline_stages[0].flags = {};
        pipeline_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        pipeline_stages[0].pName = "main";
        pipeline_stages[0].pSpecializationInfo = nullptr;

        pipeline_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_stages[1].pNext = nullptr;
        pipeline_stages[1].flags = {};
        pipeline_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        pipeline_stages[1].pName = "main";
        pipeline_stages[1].pSpecializationInfo = nullptr;

        std::vector<std::uint32_t> vertex_input_locations;
        DescriptorLayoutBindings descriptor_layout_bindings;
        std::unordered_map<std::size_t, std::vector<DescriptorBinding>> pipeline_descriptor_layout;
        { // Vertex shader.
            const auto binary    = import_spirv(info.vertex);
            const auto compiler  = spirv_cross::CompilerGLSL(binary.data(), binary.size());
            const auto resources = compiler.get_shader_resources();

            VkShaderModuleCreateInfo module_create_info;
            module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            module_create_info.pNext = nullptr;
            module_create_info.flags = {};
            module_create_info.codeSize = binary.size() * sizeof(std::uint32_t);
            module_create_info.pCode = binary.data();
            crd_vulkan_check(vkCreateShaderModule(context.device, &module_create_info, nullptr, &pipeline_stages[0].module));

            vertex_input_locations.reserve(resources.stage_inputs.size());
            for (const auto& vertex_input : resources.stage_inputs) {
                vertex_input_locations.emplace_back(compiler.get_decoration(vertex_input.id, spv::DecorationLocation));
            }
            pipeline_descriptor_layout.reserve(resources.uniform_buffers.size());
            for (const auto& uniform_buffer : resources.uniform_buffers) {
                const auto set     = compiler.get_decoration(uniform_buffer.id, spv::DecorationDescriptorSet);
                const auto binding = compiler.get_decoration(uniform_buffer.id, spv::DecorationBinding);
                descriptor_layout_bindings[uniform_buffer.name] = {
                    pipeline_descriptor_layout[set].emplace_back(DescriptorBinding{
                        .dynamic = false,
                        .index = binding,
                        .count = 1,
                        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                        .stage = VK_SHADER_STAGE_VERTEX_BIT,
                    })
                };
            }
        }

        std::vector<VkPipelineColorBlendAttachmentState> attachment_outputs;
        { // Fragment shader.
            const auto binary    = import_spirv(info.fragment);
            const auto compiler  = spirv_cross::CompilerGLSL(binary.data(), binary.size());
            const auto resources = compiler.get_shader_resources();

            VkShaderModuleCreateInfo module_create_info;
            module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            module_create_info.pNext = nullptr;
            module_create_info.flags = {};
            module_create_info.codeSize = binary.size() * sizeof(std::uint32_t);
            module_create_info.pCode = binary.data();
            crd_vulkan_check(vkCreateShaderModule(context.device, &module_create_info, nullptr, &pipeline_stages[1].module));

            VkPipelineColorBlendAttachmentState attachment;
            attachment.blendEnable = true;
            attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            attachment.colorBlendOp = VK_BLEND_OP_ADD;
            attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            attachment.alphaBlendOp = VK_BLEND_OP_ADD;
            attachment.colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT |
                VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT;
            attachment_outputs.resize(resources.stage_outputs.size(), attachment);
        }

        VkVertexInputBindingDescription vertex_binding_description;
        vertex_binding_description.binding = 0;
        vertex_binding_description.stride =
            std::accumulate(info.attributes.begin(), info.attributes.end(), 0u,
                [](const auto value, const auto attribute) noexcept {
                    return value + static_cast<std::underlying_type_t<VertexAttribute>>(attribute);
                });
        vertex_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::vector<VkVertexInputAttributeDescription> vertex_attribute_descriptions;
        vertex_attribute_descriptions.reserve(info.attributes.size());
        for (std::uint32_t location = 0, offset = 0; const auto attribute : info.attributes) {
            vertex_attribute_descriptions.push_back({
                .location = vertex_input_locations[location++],
                .binding = 0,
                .format = [attribute]() noexcept {
                    switch (attribute) {
                        case VertexAttribute::vec1: return VK_FORMAT_R32_SFLOAT;
                        case VertexAttribute::vec2: return VK_FORMAT_R32G32_SFLOAT;
                        case VertexAttribute::vec3: return VK_FORMAT_R32G32B32_SFLOAT;
                        case VertexAttribute::vec4: return VK_FORMAT_R32G32B32A32_SFLOAT;
                    }
                    crd_unreachable();
                }(),
                .offset = offset
            });
            offset += static_cast<std::underlying_type_t<VertexAttribute>>(attribute);
        }

        VkPipelineVertexInputStateCreateInfo vertex_input_state;
        vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_state.pNext = nullptr;
        vertex_input_state.flags = {};
        vertex_input_state.vertexBindingDescriptionCount = 1;
        vertex_input_state.pVertexBindingDescriptions = &vertex_binding_description;
        vertex_input_state.vertexAttributeDescriptionCount = vertex_attribute_descriptions.size();
        vertex_input_state.pVertexAttributeDescriptions = vertex_attribute_descriptions.data();

        VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
        input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_state.pNext = nullptr;
        input_assembly_state.flags = {};
        input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly_state.primitiveRestartEnable = false;

        VkViewport viewport = {};
        VkRect2D scissor = {};

        VkPipelineViewportStateCreateInfo viewport_state;
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.pNext = nullptr;
        viewport_state.flags = {};
        viewport_state.viewportCount = 1;
        viewport_state.pViewports = &viewport;
        viewport_state.scissorCount = 1;
        viewport_state.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer_state;
        rasterizer_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer_state.pNext = nullptr;
        rasterizer_state.flags = {};
        rasterizer_state.depthClampEnable = true;
        rasterizer_state.rasterizerDiscardEnable = false;
        rasterizer_state.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer_state.cullMode = VK_CULL_MODE_NONE;
        rasterizer_state.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer_state.depthBiasEnable = true;
        rasterizer_state.depthBiasConstantFactor = 0.0f;
        rasterizer_state.depthBiasClamp = 0.0f;
        rasterizer_state.depthBiasSlopeFactor = 0.0f;
        rasterizer_state.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampling_state;
        multisampling_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling_state.pNext = nullptr;
        multisampling_state.flags = {};
        multisampling_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling_state.sampleShadingEnable = true;
        multisampling_state.minSampleShading = 0.2f;
        multisampling_state.pSampleMask = nullptr;
        multisampling_state.alphaToCoverageEnable = true;
        multisampling_state.alphaToOneEnable = true;

        VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
        depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil_state.pNext = nullptr;
        depth_stencil_state.flags = {};
        depth_stencil_state.depthTestEnable = info.depth;
        depth_stencil_state.depthWriteEnable = info.depth;
        depth_stencil_state.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        depth_stencil_state.depthBoundsTestEnable = true;
        depth_stencil_state.stencilTestEnable = false;
        depth_stencil_state.front = {};
        depth_stencil_state.back = {};
        depth_stencil_state.minDepthBounds = 0.0f;
        depth_stencil_state.maxDepthBounds = 1.0f;

        VkPipelineColorBlendStateCreateInfo color_blend_state;
        color_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state.pNext = nullptr;
        color_blend_state.flags = {};
        color_blend_state.logicOpEnable = false;
        color_blend_state.logicOp = VK_LOGIC_OP_NO_OP;
        color_blend_state.attachmentCount = attachment_outputs.size();
        color_blend_state.pAttachments = attachment_outputs.data();
        color_blend_state.blendConstants[0] = 0.0f;
        color_blend_state.blendConstants[1] = 0.0f;
        color_blend_state.blendConstants[2] = 0.0f;
        color_blend_state.blendConstants[3] = 0.0f;

        VkPipelineDynamicStateCreateInfo pipeline_dynamic_states;
        pipeline_dynamic_states.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        pipeline_dynamic_states.pNext = nullptr;
        pipeline_dynamic_states.flags = {};
        pipeline_dynamic_states.dynamicStateCount = info.states.size();
        pipeline_dynamic_states.pDynamicStates = info.states.data();

        DescriptorSetLayouts set_layouts;
        set_layouts.reserve(pipeline_descriptor_layout.size());
        std::vector<VkDescriptorSetLayout> set_layout_handles;
        set_layout_handles.reserve(pipeline_descriptor_layout.size());
        for (const auto& [index, descriptors] : pipeline_descriptor_layout) {
            bool dynamic = false;
            const auto layout_hash = crd::util::hash(0, set_layouts);
            auto& layout = renderer.set_layout_cache[layout_hash];
            crd_likely_if(!layout) {
                std::vector<VkDescriptorBindingFlags> flags;
                flags.reserve(descriptors.size());
                std::vector<VkDescriptorSetLayoutBinding> bindings;
                bindings.reserve(descriptors.size());
                for (const auto& binding : descriptors) {
                    flags.emplace_back();
                    if (binding.dynamic) {
                        dynamic = true;
                        flags.back() =
                            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                            VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
                    }
                    bindings.push_back({
                        .binding = binding.index,
                        .descriptorType = binding.type,
                        .descriptorCount = binding.count,
                        .stageFlags = binding.stage,
                        .pImmutableSamplers = nullptr
                    });
                }

                VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags;
                binding_flags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
                binding_flags.pNext = nullptr;
                binding_flags.bindingCount = flags.size();
                binding_flags.pBindingFlags = flags.data();

                VkDescriptorSetLayoutCreateInfo layout_info;
                layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                layout_info.pNext = &binding_flags;
                layout_info.flags = {};
                layout_info.bindingCount = bindings.size();
                layout_info.pBindings = bindings.data();
                crd_vulkan_check(vkCreateDescriptorSetLayout(context.device, &layout_info, nullptr, &layout));
            }
            set_layouts.push_back({ layout, dynamic });
            set_layout_handles.emplace_back(layout);
        }
        pipeline.descriptors = std::move(set_layouts);
        pipeline.bindings = std::move(descriptor_layout_bindings);
        VkPipelineLayoutCreateInfo layout_create_info;
        layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_create_info.pNext = nullptr;
        layout_create_info.flags = {};
        layout_create_info.setLayoutCount = set_layout_handles.size();
        layout_create_info.pSetLayouts = set_layout_handles.data();
        layout_create_info.pushConstantRangeCount = 0;
        layout_create_info.pPushConstantRanges = nullptr;
        crd_vulkan_check(vkCreatePipelineLayout(context.device, &layout_create_info, nullptr, &pipeline.layout));

        VkGraphicsPipelineCreateInfo pipeline_create_info;
        pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_create_info.pNext = nullptr;
        pipeline_create_info.flags = {};
        pipeline_create_info.stageCount = 2;
        pipeline_create_info.pStages = pipeline_stages;
        pipeline_create_info.pVertexInputState = &vertex_input_state;
        pipeline_create_info.pInputAssemblyState = &input_assembly_state;
        pipeline_create_info.pTessellationState = nullptr;
        pipeline_create_info.pViewportState = &viewport_state;
        pipeline_create_info.pRasterizationState = &rasterizer_state;
        pipeline_create_info.pMultisampleState = &multisampling_state;
        pipeline_create_info.pDepthStencilState = &depth_stencil_state;
        pipeline_create_info.pColorBlendState = &color_blend_state;
        pipeline_create_info.pDynamicState = &pipeline_dynamic_states;
        pipeline_create_info.layout = pipeline.layout;
        pipeline_create_info.renderPass = info.render_pass->handle;
        pipeline_create_info.subpass = info.subpass;
        pipeline_create_info.basePipelineHandle = nullptr;
        pipeline_create_info.basePipelineIndex = -1;

        crd_vulkan_check(vkCreateGraphicsPipelines(context.device, nullptr, 1, &pipeline_create_info, nullptr, &pipeline.handle));
        vkDestroyShaderModule(context.device, pipeline_stages[0].module, nullptr);
        vkDestroyShaderModule(context.device, pipeline_stages[1].module, nullptr);
        return pipeline;
    }

    crd_module void destroy_pipeline(const Context& context, Pipeline& pipeline) noexcept {
        vkDestroyPipelineLayout(context.device, pipeline.layout, nullptr);
        vkDestroyPipeline(context.device, pipeline.handle, nullptr);
        pipeline = {};
    }

    crd_nodiscard crd_module DescriptorSetLayout Pipeline::set(std::size_t index) const noexcept {
        return descriptors.at(index);
    }
} // namespace crd::core
