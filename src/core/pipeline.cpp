#include <corundum/core/render_pass.hpp>
#include <corundum/core/utilities.hpp>
#include <corundum/core/pipeline.hpp>
#include <corundum/core/renderer.hpp>
#include <corundum/core/dispatch.hpp>
#include <corundum/core/context.hpp>

#include <corundum/detail/file_view.hpp>
#include <corundum/detail/hash.hpp>

#if defined(crd_enable_profiling)
    #include <Tracy.hpp>
#endif

#include <spdlog/spdlog.h>

#include <spirv_glsl.hpp>
#include <spirv.hpp>

#include <algorithm>
#include <numeric>
#include <cstring>
#include <map>

namespace crd {
    namespace spvc = spirv_cross;

    crd_nodiscard static inline std::uint32_t aligned_size(std::uint32_t size, std::uint32_t alignment) noexcept {
        crd_profile_scoped();
        return (size + alignment - 1) & ~(alignment - 1);
    }

    crd_nodiscard static inline std::vector<std::uint32_t> import_spirv(const char* path) noexcept {
        crd_profile_scoped();
        auto file = dtl::make_file_view(path);
        std::vector<std::uint32_t> code(file.size / sizeof(std::uint32_t));
        std::memcpy(code.data(), file.data, file.size);
        dtl::destroy_file_view(file);
        return code;
    }

    crd_nodiscard crd_module GraphicsPipeline make_pipeline(Renderer& renderer, GraphicsPipeline::CreateInfo&& info) noexcept {
        crd_profile_scoped();
        const auto* context = renderer.context;
        spdlog::info("loading vertex shader: \"{}\"", info.vertex);
        if (info.geometry) {
            spdlog::info("loading geometry shader: \"{}\"", info.geometry);
        }
        if (info.fragment) {
            spdlog::info("loading fragment shader: \"{}\"", info.fragment);
        }
        GraphicsPipeline pipeline;
        pipeline.context = context;
        pipeline.renderer = &renderer;
        crd_assert(info.vertex, "vertex shader not present");
        VkPipelineShaderStageCreateInfo vertex_stage;
        vertex_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertex_stage.pNext = nullptr;
        vertex_stage.flags = {};
        vertex_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertex_stage.pName = "main";
        vertex_stage.pSpecializationInfo = nullptr;

        VkPipelineShaderStageCreateInfo geometry_stage;
        if (info.geometry) {
            crd_assert(context->gpu.features.geometryShader, "geometry shader requested but not supported");
            geometry_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            geometry_stage.pNext = nullptr;
            geometry_stage.flags = {};
            geometry_stage.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
            geometry_stage.pName = "main";
            geometry_stage.pSpecializationInfo = nullptr;
        }

        VkPipelineShaderStageCreateInfo fragment_stage;
        if (info.fragment) {
            fragment_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            fragment_stage.pNext = nullptr;
            fragment_stage.flags = {};
            fragment_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            fragment_stage.pName = "main";
            fragment_stage.pSpecializationInfo = nullptr;
        }

        VkPushConstantRange push_constant_range;
        push_constant_range.stageFlags = {};
        push_constant_range.offset = 0;
        push_constant_range.size = 0;
        std::vector<std::uint32_t> vertex_input_locations;
        DescriptorLayoutBindings descriptor_layout_bindings;
        std::map<std::size_t, std::vector<DescriptorBinding>> pipeline_descriptor_layout;
        crd_assert(info.vertex, "vertex shader not present");
        {
            const auto binary = import_spirv(info.vertex);
            const auto compiler = spvc::CompilerGLSL(binary.data(), binary.size());
            const auto resources = compiler.get_shader_resources();

            VkShaderModuleCreateInfo module_create_info;
            module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            module_create_info.pNext = nullptr;
            module_create_info.flags = {};
            module_create_info.codeSize = size_bytes(binary);
            module_create_info.pCode = binary.data();
            crd_vulkan_check(vkCreateShaderModule(context->device, &module_create_info, nullptr, &vertex_stage.module));

            for (const auto& buffer : resources.uniform_buffers) {
                const auto set = compiler.get_decoration(buffer.id, spv::DecorationDescriptorSet);
                const auto binding = compiler.get_decoration(buffer.id, spv::DecorationBinding);
                auto& descriptor = pipeline_descriptor_layout[set];
                const auto found =
                    std::find_if(descriptor.begin(), descriptor.end(), [binding](const auto& each) {
                        return each.index == binding;
                    });
                if (found != descriptor.end()) {
                    found->stage |= VK_SHADER_STAGE_VERTEX_BIT;
                } else {
                    pipeline_descriptor_layout[set].emplace_back(
                        descriptor_layout_bindings[buffer.name] = {
                            .dynamic = false,
                            .index = binding,
                            .count = 1,
                            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            .stage = VK_SHADER_STAGE_VERTEX_BIT
                        });
                }
            }
            for (const auto& buffer : resources.storage_buffers) {
                const auto set = compiler.get_decoration(buffer.id, spv::DecorationDescriptorSet);
                const auto binding = compiler.get_decoration(buffer.id, spv::DecorationBinding);
                auto& descriptor = pipeline_descriptor_layout[set];
                const auto found =
                    std::find_if(descriptor.begin(), descriptor.end(), [binding](const auto& each) {
                        return each.index == binding;
                    });
                if (found != descriptor.end()) {
                    found->stage |= VK_SHADER_STAGE_VERTEX_BIT;
                } else {
                    pipeline_descriptor_layout[set].emplace_back(
                        descriptor_layout_bindings[buffer.name] = {
                            .dynamic = false,
                            .index = binding,
                            .count = 1,
                            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                            .stage = VK_SHADER_STAGE_VERTEX_BIT
                        });
                }
            }
            for (const auto& push_constant : resources.push_constant_buffers) {
                const auto& type = compiler.get_type(push_constant.type_id);
                push_constant_range.size = compiler.get_declared_struct_size(type);
                push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            }
        }

        if (info.geometry) {
            const auto binary = import_spirv(info.geometry);
            const auto compiler = spvc::CompilerGLSL(binary.data(), binary.size());
            const auto resources = compiler.get_shader_resources();

            VkShaderModuleCreateInfo module_create_info;
            module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            module_create_info.pNext = nullptr;
            module_create_info.flags = {};
            module_create_info.codeSize = size_bytes(binary);
            module_create_info.pCode = binary.data();
            crd_vulkan_check(vkCreateShaderModule(context->device, &module_create_info, nullptr, &geometry_stage.module));

            for (const auto& buffer : resources.uniform_buffers) {
                const auto set = compiler.get_decoration(buffer.id, spv::DecorationDescriptorSet);
                const auto binding = compiler.get_decoration(buffer.id, spv::DecorationBinding);
                auto& descriptor = pipeline_descriptor_layout[set];
                const auto found =
                    std::find_if(descriptor.begin(), descriptor.end(), [binding](const auto& each) {
                        return each.index == binding;
                    });
                if (found != descriptor.end()) {
                    found->stage |= VK_SHADER_STAGE_GEOMETRY_BIT;
                } else {
                    pipeline_descriptor_layout[set].emplace_back(
                        descriptor_layout_bindings[buffer.name] = {
                            .dynamic = false,
                            .index = binding,
                            .count = 1,
                            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            .stage = VK_SHADER_STAGE_GEOMETRY_BIT
                        });
                }
            }
            for (const auto& buffer : resources.storage_buffers) {
                const auto set = compiler.get_decoration(buffer.id, spv::DecorationDescriptorSet);
                const auto binding = compiler.get_decoration(buffer.id, spv::DecorationBinding);
                auto& descriptor = pipeline_descriptor_layout[set];
                const auto found =
                    std::find_if(descriptor.begin(), descriptor.end(), [binding](const auto& each) {
                        return each.index == binding;
                    });
                if (found != descriptor.end()) {
                    found->stage |= VK_SHADER_STAGE_GEOMETRY_BIT;
                } else {
                    pipeline_descriptor_layout[set].emplace_back(
                        descriptor_layout_bindings[buffer.name] = {
                            .dynamic = false,
                            .index = binding,
                            .count = 1,
                            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                            .stage = VK_SHADER_STAGE_GEOMETRY_BIT
                        });
                }
            }
            for (const auto& push_constant : resources.push_constant_buffers) {
                const auto& type = compiler.get_type(push_constant.type_id);
                push_constant_range.size = compiler.get_declared_struct_size(type);
                push_constant_range.stageFlags |= VK_SHADER_STAGE_GEOMETRY_BIT;
            }
        }

        std::vector<VkPipelineColorBlendAttachmentState> attachment_outputs;
        if (info.fragment) {
            const auto binary = import_spirv(info.fragment);
            const auto compiler = spvc::CompilerGLSL(binary.data(), binary.size());
            const auto resources = compiler.get_shader_resources();

            VkShaderModuleCreateInfo module_create_info;
            module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            module_create_info.pNext = nullptr;
            module_create_info.flags = {};
            module_create_info.codeSize = size_bytes(binary);
            module_create_info.pCode = binary.data();
            crd_vulkan_check(vkCreateShaderModule(context->device, &module_create_info, nullptr, &fragment_stage.module));

            VkPipelineColorBlendAttachmentState attachment;
            attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            attachment.colorBlendOp = VK_BLEND_OP_ADD;
            attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            attachment.alphaBlendOp = VK_BLEND_OP_ADD;
            attachment.colorWriteMask = {};
            crd_assert(resources.stage_outputs.size() == info.attachments.size(), "output attachment count differ");
            for (std::uint32_t i = 0; const auto& outputs : resources.stage_outputs) {
                const auto& type = compiler.get_type(outputs.type_id);
                switch (info.attachments[i++]) {
                    case color_attachment_auto: {
                        attachment.blendEnable = type.vecsize == 4;
                    } break;
                    case color_attachment_disable_blend: {
                        attachment.blendEnable = false;
                    } break;
                }
                switch (type.vecsize) {
                    case 4: attachment.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;
                    case 3: attachment.colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
                    case 2: attachment.colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
                    case 1: attachment.colorWriteMask |= VK_COLOR_COMPONENT_R_BIT;
                }
            }
            attachment_outputs.resize(resources.stage_outputs.size(), attachment);
            for (const auto& input_attachment : resources.subpass_inputs) {
                const auto set = compiler.get_decoration(input_attachment.id, spv::DecorationDescriptorSet);
                const auto binding = compiler.get_decoration(input_attachment.id, spv::DecorationBinding);
                pipeline_descriptor_layout[set].emplace_back(
                    descriptor_layout_bindings[input_attachment.name] = {
                        .dynamic = false,
                        .index = binding,
                        .count = 1,
                        .type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                        .stage = VK_SHADER_STAGE_FRAGMENT_BIT
                    });
            }
            for (const auto& buffer : resources.uniform_buffers) {
                const auto set = compiler.get_decoration(buffer.id, spv::DecorationDescriptorSet);
                const auto binding = compiler.get_decoration(buffer.id, spv::DecorationBinding);
                auto& descriptor = pipeline_descriptor_layout[set];
                const auto found =
                    std::find_if(descriptor.begin(), descriptor.end(), [binding](const auto& each) {
                        return each.index == binding;
                    });
                if (found != descriptor.end()) {
                    found->stage |= VK_SHADER_STAGE_FRAGMENT_BIT;
                } else {
                    pipeline_descriptor_layout[set].emplace_back(
                        descriptor_layout_bindings[buffer.name] = {
                            .dynamic = false,
                            .index = binding,
                            .count = 1,
                            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            .stage = VK_SHADER_STAGE_FRAGMENT_BIT
                        });
                }
            }
            for (const auto& buffer : resources.storage_buffers) {
                const auto set = compiler.get_decoration(buffer.id, spv::DecorationDescriptorSet);
                const auto binding = compiler.get_decoration(buffer.id, spv::DecorationBinding);
                auto& descriptor = pipeline_descriptor_layout[set];
                const auto found =
                    std::find_if(descriptor.begin(), descriptor.end(), [binding](const auto& each) {
                        return each.index == binding;
                    });
                if (found != descriptor.end()) {
                    found->stage |= VK_SHADER_STAGE_FRAGMENT_BIT;
                } else {
                    pipeline_descriptor_layout[set].emplace_back(
                        descriptor_layout_bindings[buffer.name] = {
                            .dynamic = false,
                            .index = binding,
                            .count = 1,
                            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                            .stage = VK_SHADER_STAGE_FRAGMENT_BIT
                        });
                }
            }
            for (const auto& textures : resources.sampled_images) {
                const auto set = compiler.get_decoration(textures.id, spv::DecorationDescriptorSet);
                const auto binding = compiler.get_decoration(textures.id, spv::DecorationBinding);
                const auto& image_type = compiler.get_type(textures.type_id);
                const bool is_array = !image_type.array.empty();
                const bool is_dynamic = is_array && image_type.array[0] == 0;
                const auto max_samplers = max_bound_samplers(*context);
                crd_assert(!is_dynamic || context->extensions.descriptor_indexing,
                           "shader uses descriptor indexing but GPU extension is not supported");
                pipeline_descriptor_layout[set].emplace_back(
                    descriptor_layout_bindings[textures.name] = {
                        .dynamic = is_dynamic,
                        .index = binding,
                        .count = !is_array ? 1 : (is_dynamic ? max_samplers : image_type.array[0]),
                        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .stage = VK_SHADER_STAGE_FRAGMENT_BIT
                    });
            }
            for (const auto& push_constant : resources.push_constant_buffers) {
                const auto& type = compiler.get_type(push_constant.type_id);
                push_constant_range.size = compiler.get_declared_struct_size(type);
                push_constant_range.stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
            }
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
        for (std::uint32_t offset = 0, location = 0; const auto attribute : info.attributes) {
            vertex_attribute_descriptions.push_back({
                .location = location++,
                .binding = 0,
                .format = [&]() noexcept {
                    switch (attribute) {
                        case vertex_attribute_vec1: return VK_FORMAT_R32_SFLOAT;
                        case vertex_attribute_vec2: return VK_FORMAT_R32G32_SFLOAT;
                        case vertex_attribute_vec3: return VK_FORMAT_R32G32B32_SFLOAT;
                        case vertex_attribute_vec4: return VK_FORMAT_R32G32B32A32_SFLOAT;
                    }
                    crd_unreachable();
                }(),
                .offset = offset
            });
            offset += static_cast<std::underlying_type_t<VertexAttribute>>(attribute);
        }

        VkPipelineVertexInputStateCreateInfo vertex_input_state = {};
        vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_state.pNext = nullptr;
        vertex_input_state.flags = {};
        if (!vertex_attribute_descriptions.empty()) {
            vertex_input_state.vertexBindingDescriptionCount = 1;
            vertex_input_state.pVertexBindingDescriptions = &vertex_binding_description;
            vertex_input_state.vertexAttributeDescriptionCount = vertex_attribute_descriptions.size();
            vertex_input_state.pVertexAttributeDescriptions = vertex_attribute_descriptions.data();
        }

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
        rasterizer_state.depthClampEnable = false;
        rasterizer_state.rasterizerDiscardEnable = false;
        rasterizer_state.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer_state.cullMode = info.cull;
        rasterizer_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer_state.depthBiasEnable = false;
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
        multisampling_state.alphaToCoverageEnable = false;
        multisampling_state.alphaToOneEnable = false;

        VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
        depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil_state.pNext = nullptr;
        depth_stencil_state.flags = {};
        depth_stencil_state.depthTestEnable = info.depth.test;
        depth_stencil_state.depthWriteEnable = info.depth.write;
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
            std::uint32_t max_bindings = 0;
            const auto layout_hash = dtl::hash(0, descriptors);
            auto& layout = renderer.set_layout_cache[layout_hash];
            crd_unlikely_if(!layout) {
                std::vector<VkDescriptorBindingFlags> flags;
                flags.reserve(descriptors.size());
                std::vector<VkDescriptorSetLayoutBinding> bindings;
                bindings.reserve(descriptors.size());
                for (const auto& binding : descriptors) {
                    flags.emplace_back();
                    if (binding.dynamic) {
                        dynamic = true;
                        max_bindings = binding.count;
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
                crd_vulkan_check(vkCreateDescriptorSetLayout(context->device, &layout_info, nullptr, &layout));
            }
            set_layouts.push_back({ layout, max_bindings, dynamic });
            set_layout_handles.emplace_back(layout);
        }
        pipeline.type = Pipeline::type_graphics;
        pipeline.layout.sets = std::move(set_layouts);
        pipeline.bindings = std::move(descriptor_layout_bindings);
        VkPipelineLayoutCreateInfo pipeline_layout_info;
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.pNext = nullptr;
        pipeline_layout_info.flags = {};
        pipeline_layout_info.setLayoutCount = set_layout_handles.size();
        pipeline_layout_info.pSetLayouts = set_layout_handles.data();
        pipeline_layout_info.pushConstantRangeCount = push_constant_range.size != 0;
        pipeline_layout_info.pPushConstantRanges = &push_constant_range;
        crd_vulkan_check(vkCreatePipelineLayout(context->device, &pipeline_layout_info, nullptr, &pipeline.layout.pipeline));

        std::vector<VkPipelineShaderStageCreateInfo> pipeline_stages;
        pipeline_stages.reserve(3);
        pipeline_stages.emplace_back(vertex_stage);
        if (info.geometry) { pipeline_stages.emplace_back(geometry_stage); }
        if (info.fragment) { pipeline_stages.emplace_back(fragment_stage); }

        VkGraphicsPipelineCreateInfo pipeline_info;
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.pNext = nullptr;
        pipeline_info.flags = {};
        pipeline_info.stageCount = pipeline_stages.size();
        pipeline_info.pStages = pipeline_stages.data();
        pipeline_info.pVertexInputState = &vertex_input_state;
        pipeline_info.pInputAssemblyState = &input_assembly_state;
        pipeline_info.pTessellationState = nullptr;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.pRasterizationState = &rasterizer_state;
        pipeline_info.pMultisampleState = &multisampling_state;
        pipeline_info.pDepthStencilState = &depth_stencil_state;
        pipeline_info.pColorBlendState = &color_blend_state;
        pipeline_info.pDynamicState = &pipeline_dynamic_states;
        pipeline_info.layout = pipeline.layout.pipeline;
        pipeline_info.renderPass = info.render_pass->handle;
        pipeline_info.subpass = info.subpass;
        pipeline_info.basePipelineHandle = nullptr;
        pipeline_info.basePipelineIndex = -1;

        crd_vulkan_check(vkCreateGraphicsPipelines(context->device, nullptr, 1, &pipeline_info, nullptr, &pipeline.handle));
        for (const auto& stage : pipeline_stages) {
            vkDestroyShaderModule(context->device, stage.module, nullptr);
        }
        spdlog::info("pipeline created successfully");
        return pipeline;
    }

    crd_nodiscard crd_module ComputePipeline make_pipeline(Renderer& renderer, ComputePipeline::CreateInfo&& info) noexcept {
        crd_profile_scoped();
        const auto* context = renderer.context;
        ComputePipeline pipeline;
        pipeline.context = context;
        pipeline.renderer = &renderer;
        spdlog::info("loading compute shader: \"{}\"", info.compute);
        const auto binary = import_spirv(info.compute);
        VkShaderModuleCreateInfo compute_module;
        compute_module.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        compute_module.pNext = nullptr;
        compute_module.flags = {};
        compute_module.codeSize = size_bytes(binary);
        compute_module.pCode = binary.data();

        VkPipelineShaderStageCreateInfo compute_stage;
        compute_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        compute_stage.pNext = nullptr;
        compute_stage.flags = {};
        compute_stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        compute_stage.pName = "main";
        compute_stage.pSpecializationInfo = nullptr;
        crd_vulkan_check(vkCreateShaderModule(context->device, &compute_module, nullptr, &compute_stage.module));

        VkPushConstantRange push_constant_range;
        push_constant_range.stageFlags = {};
        push_constant_range.offset = 0;
        push_constant_range.size = 0;
        DescriptorLayoutBindings descriptor_layout_bindings;
        std::map<std::size_t, std::vector<DescriptorBinding>> pipeline_descriptor_layout;
        { // Compute shader.
            const auto compiler = spvc::CompilerGLSL(binary.data(), binary.size());
            const auto resources = compiler.get_shader_resources();
            for (const auto& uniform_buffer : resources.uniform_buffers) {
                const auto set = compiler.get_decoration(uniform_buffer.id, spv::DecorationDescriptorSet);
                const auto binding = compiler.get_decoration(uniform_buffer.id, spv::DecorationBinding);
                pipeline_descriptor_layout[set].emplace_back(
                    descriptor_layout_bindings[uniform_buffer.name] = {
                        .dynamic = false,
                        .index = binding,
                        .count = 1,
                        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                        .stage = VK_SHADER_STAGE_COMPUTE_BIT
                    });
            }
            for (const auto& storage_buffer : resources.storage_buffers) {
                const auto set = compiler.get_decoration(storage_buffer.id, spv::DecorationDescriptorSet);
                const auto binding = compiler.get_decoration(storage_buffer.id, spv::DecorationBinding);
                pipeline_descriptor_layout[set].emplace_back(
                    descriptor_layout_bindings[storage_buffer.name] = {
                        .dynamic = false,
                        .index = binding,
                        .count = 1,
                        .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        .stage = VK_SHADER_STAGE_COMPUTE_BIT
                    });
            }
            for (const auto& textures : resources.sampled_images) {
                const auto set = compiler.get_decoration(textures.id, spv::DecorationDescriptorSet);
                const auto binding = compiler.get_decoration(textures.id, spv::DecorationBinding);
                const auto& image_type = compiler.get_type(textures.type_id);
                const bool is_array = !image_type.array.empty();
                const bool is_dynamic = is_array && image_type.array[0] == 0;
                const auto max_samplers = max_bound_samplers(*context);
                pipeline_descriptor_layout[set].emplace_back(
                    descriptor_layout_bindings[textures.name] = {
                        .dynamic = is_dynamic,
                        .index = binding,
                        .count = !is_array ? 1 : (is_dynamic ? max_samplers : image_type.array[0]),
                        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .stage = VK_SHADER_STAGE_COMPUTE_BIT
                    });
            }
            for (const auto& push_constant : resources.push_constant_buffers) {
                const auto& type = compiler.get_type(push_constant.type_id);
                push_constant_range.size = compiler.get_declared_struct_size(type);
                push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            }
        }
        DescriptorSetLayouts set_layouts;
        set_layouts.reserve(pipeline_descriptor_layout.size());
        std::vector<VkDescriptorSetLayout> set_layout_handles;
        set_layout_handles.reserve(pipeline_descriptor_layout.size());
        for (const auto& [index, descriptors] : pipeline_descriptor_layout) {
            bool dynamic = false;
            std::uint32_t max_bindings = 0;
            const auto layout_hash = dtl::hash(0, descriptors);
            auto& layout = renderer.set_layout_cache[layout_hash];
            crd_unlikely_if(!layout) {
                std::vector<VkDescriptorBindingFlags> flags;
                flags.reserve(descriptors.size());
                std::vector<VkDescriptorSetLayoutBinding> bindings;
                bindings.reserve(descriptors.size());
                for (const auto& binding : descriptors) {
                    flags.emplace_back();
                    if (binding.dynamic) {
                        dynamic = true;
                        max_bindings = binding.count;
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
                layout_info.pNext = nullptr;
                if (context->extensions.descriptor_indexing) {
                    layout_info.pNext = &binding_flags;
                }
                layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                layout_info.flags = {};
                layout_info.bindingCount = bindings.size();
                layout_info.pBindings = bindings.data();
                crd_vulkan_check(vkCreateDescriptorSetLayout(context->device, &layout_info, nullptr, &layout));
            }
            set_layouts.push_back({ layout, max_bindings, dynamic });
            set_layout_handles.emplace_back(layout);
        }
        pipeline.type = Pipeline::type_compute;
        pipeline.layout.sets = std::move(set_layouts);
        pipeline.bindings = std::move(descriptor_layout_bindings);
        VkPipelineLayoutCreateInfo pipeline_layout_info;
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.pNext = nullptr;
        pipeline_layout_info.flags = {};
        pipeline_layout_info.setLayoutCount = set_layout_handles.size();
        pipeline_layout_info.pSetLayouts = set_layout_handles.data();
        pipeline_layout_info.pushConstantRangeCount = push_constant_range.size != 0;
        pipeline_layout_info.pPushConstantRanges = &push_constant_range;
        crd_vulkan_check(vkCreatePipelineLayout(context->device, &pipeline_layout_info, nullptr, &pipeline.layout.pipeline));

        VkComputePipelineCreateInfo pipeline_info;
        pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_info.pNext = nullptr;
        pipeline_info.flags = {};
        pipeline_info.stage = compute_stage;
        pipeline_info.layout = pipeline.layout.pipeline;
        pipeline_info.basePipelineHandle = nullptr;
        pipeline_info.basePipelineIndex = -1;
        crd_vulkan_check(vkCreateComputePipelines(context->device, nullptr, 1, &pipeline_info, nullptr, &pipeline.handle));
        vkDestroyShaderModule(context->device, compute_stage.module, nullptr);
        spdlog::info("pipeline created successfully");
        return pipeline;
    }

    // TODO: Add support for multiple shaders in one SBT
    crd_nodiscard crd_module RayTracingPipeline make_pipeline(Renderer& renderer, RayTracingPipeline::CreateInfo&& info) noexcept {
#if defined(crd_enable_raytracing)
        crd_profile_scoped();
        const auto* context = renderer.context;
        RayTracingPipeline pipeline;
        pipeline.context = context;
        pipeline.renderer = &renderer;
        std::vector<VkRayTracingShaderGroupCreateInfoKHR> pipeline_groups;
        pipeline_groups.reserve(3);
        std::vector<VkPipelineShaderStageCreateInfo> pipeline_stages;
        pipeline_stages.reserve(3);

        VkPushConstantRange push_constant_range;
        push_constant_range.stageFlags = {};
        push_constant_range.offset = 0;
        push_constant_range.size = 0;
        DescriptorLayoutBindings descriptor_layout_bindings;
        std::map<std::size_t, std::vector<DescriptorBinding>> pipeline_descriptor_layout;
        const auto store_resources = [&](const spvc::CompilerGLSL& compiler, VkShaderStageFlags stage) {
            const auto resources = compiler.get_shader_resources();
            for (const auto& buffer : resources.uniform_buffers) {
                const auto set = compiler.get_decoration(buffer.id, spv::DecorationDescriptorSet);
                const auto binding = compiler.get_decoration(buffer.id, spv::DecorationBinding);
                auto& descriptor = pipeline_descriptor_layout[set];
                const auto found =
                    std::find_if(descriptor.begin(), descriptor.end(), [binding](const auto& each) {
                        return each.index == binding;
                    });
                if (found != descriptor.end()) {
                    found->stage |= stage;
                } else {
                    pipeline_descriptor_layout[set].emplace_back(
                        descriptor_layout_bindings[buffer.name] = {
                            .dynamic = false,
                            .index = binding,
                            .count = 1,
                            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            .stage = stage
                        });
                }
            }
            for (const auto& buffer : resources.storage_buffers) {
                const auto set = compiler.get_decoration(buffer.id, spv::DecorationDescriptorSet);
                const auto binding = compiler.get_decoration(buffer.id, spv::DecorationBinding);
                auto& descriptor = pipeline_descriptor_layout[set];
                const auto found =
                    std::find_if(descriptor.begin(), descriptor.end(), [binding](const auto& each) {
                        return each.index == binding;
                    });
                if (found != descriptor.end()) {
                    found->stage |= stage;
                } else {
                    pipeline_descriptor_layout[set].emplace_back(
                        descriptor_layout_bindings[buffer.name] = {
                            .dynamic = false,
                            .index = binding,
                            .count = 1,
                            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                            .stage = stage
                        });
                }
            }
            for (const auto& image : resources.storage_images) {
                const auto set = compiler.get_decoration(image.id, spv::DecorationDescriptorSet);
                const auto binding = compiler.get_decoration(image.id, spv::DecorationBinding);
                const auto& image_type = compiler.get_type(image.type_id);
                const bool is_array = !image_type.array.empty();
                const bool is_dynamic = is_array && image_type.array[0] == 0;
                const auto max_images = 1024; // TODO: Don't hardcode
                auto& descriptor = pipeline_descriptor_layout[set];
                const auto found =
                    std::find_if(descriptor.begin(), descriptor.end(), [binding](const auto& each) {
                        return each.index == binding;
                    });
                if (found != descriptor.end()) {
                    found->stage |= stage;
                } else {
                    pipeline_descriptor_layout[set].emplace_back(
                        descriptor_layout_bindings[image.name] = {
                            .dynamic = is_dynamic,
                            .index = binding,
                            .count = !is_array ? 1 : (is_dynamic ? max_images : image_type.array[0]),
                            .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                            .stage = stage
                        });
                }
            }
            for (const auto& tlas : resources.acceleration_structures) {
                const auto set = compiler.get_decoration(tlas.id, spv::DecorationDescriptorSet);
                const auto binding = compiler.get_decoration(tlas.id, spv::DecorationBinding);
                const auto& as_type = compiler.get_type(tlas.type_id);
                const bool is_array = !as_type.array.empty();
                const bool is_dynamic = is_array && as_type.array[0] == 0;
                const auto max_bound = 1024; // TODO: Don't hardcode
                auto& descriptor = pipeline_descriptor_layout[set];
                const auto found =
                    std::find_if(descriptor.begin(), descriptor.end(), [binding](const auto& each) {
                        return each.index == binding;
                    });
                if (found != descriptor.end()) {
                    found->stage |= stage;
                } else {
                    pipeline_descriptor_layout[set].emplace_back(
                        descriptor_layout_bindings[tlas.name] = {
                            .dynamic = is_dynamic,
                            .index = binding,
                            .count = !is_array ? 1 : (is_dynamic ? max_bound : as_type.array[0]),
                            .type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                            .stage = stage
                        });
                }
            }
            for (const auto& textures : resources.sampled_images) {
                const auto set = compiler.get_decoration(textures.id, spv::DecorationDescriptorSet);
                const auto binding = compiler.get_decoration(textures.id, spv::DecorationBinding);
                const auto& image_type = compiler.get_type(textures.type_id);
                const bool is_array = !image_type.array.empty();
                const bool is_dynamic = is_array && image_type.array[0] == 0;
                const auto max_samplers = max_bound_samplers(*context);
                auto& descriptor = pipeline_descriptor_layout[set];
                const auto found =
                    std::find_if(descriptor.begin(), descriptor.end(), [binding](const auto& each) {
                        return each.index == binding;
                    });
                if (found != descriptor.end()) {
                    found->stage |= stage;
                } else {
                    pipeline_descriptor_layout[set].emplace_back(
                        descriptor_layout_bindings[textures.name] = {
                            .dynamic = is_dynamic,
                            .index = binding,
                            .count = !is_array ? 1 : (is_dynamic ? max_samplers : image_type.array[0]),
                            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            .stage = stage
                        });
                }
            }
            for (const auto& push_constant : resources.push_constant_buffers) {
                const auto& type = compiler.get_type(push_constant.type_id);
                push_constant_range.size = compiler.get_declared_struct_size(type);
                push_constant_range.stageFlags = stage;
            }
        };
        { // Ray Generetion
            const auto binary = import_spirv(info.raygen);
            const auto compiler = spvc::CompilerGLSL(binary.data(), binary.size());

            VkShaderModuleCreateInfo module_info;
            module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            module_info.pNext = nullptr;
            module_info.flags = {};
            module_info.codeSize = size_bytes(binary);
            module_info.pCode = binary.data();

            VkPipelineShaderStageCreateInfo pipeline_stage;
            pipeline_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            pipeline_stage.pNext = nullptr;
            pipeline_stage.flags = {};
            pipeline_stage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
            pipeline_stage.pName = "main";
            pipeline_stage.pSpecializationInfo = nullptr;
            crd_vulkan_check(vkCreateShaderModule(context->device, &module_info, nullptr, &pipeline_stage.module));
            pipeline_stages.emplace_back(pipeline_stage);

            VkRayTracingShaderGroupCreateInfoKHR pipeline_group;
            pipeline_group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            pipeline_group.pNext = nullptr;
            pipeline_group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            pipeline_group.generalShader = 0;
            pipeline_group.closestHitShader = VK_SHADER_UNUSED_KHR;
            pipeline_group.anyHitShader = VK_SHADER_UNUSED_KHR;
            pipeline_group.intersectionShader = VK_SHADER_UNUSED_KHR;
            pipeline_group.pShaderGroupCaptureReplayHandle = nullptr;
            pipeline_groups.emplace_back(pipeline_group);

            store_resources(compiler, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
        }
        { // Ray Miss
            const auto binary = import_spirv(info.raymiss);
            const auto compiler = spvc::CompilerGLSL(binary.data(), binary.size());

            VkShaderModuleCreateInfo module_info;
            module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            module_info.pNext = nullptr;
            module_info.flags = {};
            module_info.codeSize = size_bytes(binary);
            module_info.pCode = binary.data();

            VkPipelineShaderStageCreateInfo pipeline_stage;
            pipeline_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            pipeline_stage.pNext = nullptr;
            pipeline_stage.flags = {};
            pipeline_stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
            pipeline_stage.pName = "main";
            pipeline_stage.pSpecializationInfo = nullptr;
            crd_vulkan_check(vkCreateShaderModule(context->device, &module_info, nullptr, &pipeline_stage.module));
            pipeline_stages.emplace_back(pipeline_stage);

            VkRayTracingShaderGroupCreateInfoKHR pipeline_group;
            pipeline_group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            pipeline_group.pNext = nullptr;
            pipeline_group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            pipeline_group.generalShader = 1;
            pipeline_group.closestHitShader = VK_SHADER_UNUSED_KHR;
            pipeline_group.anyHitShader = VK_SHADER_UNUSED_KHR;
            pipeline_group.intersectionShader = VK_SHADER_UNUSED_KHR;
            pipeline_group.pShaderGroupCaptureReplayHandle = nullptr;
            pipeline_groups.emplace_back(pipeline_group);

            store_resources(compiler, VK_SHADER_STAGE_MISS_BIT_KHR);
        }
        { // Ray Closest Hit
            const auto binary = import_spirv(info.raychit);
            const auto compiler = spvc::CompilerGLSL(binary.data(), binary.size());

            VkShaderModuleCreateInfo module_info;
            module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            module_info.pNext = nullptr;
            module_info.flags = {};
            module_info.codeSize = size_bytes(binary);
            module_info.pCode = binary.data();

            VkPipelineShaderStageCreateInfo pipeline_stage;
            pipeline_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            pipeline_stage.pNext = nullptr;
            pipeline_stage.flags = {};
            pipeline_stage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
            pipeline_stage.pName = "main";
            pipeline_stage.pSpecializationInfo = nullptr;
            crd_vulkan_check(vkCreateShaderModule(context->device, &module_info, nullptr, &pipeline_stage.module));
            pipeline_stages.emplace_back(pipeline_stage);

            VkRayTracingShaderGroupCreateInfoKHR pipeline_group;
            pipeline_group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            pipeline_group.pNext = nullptr;
            pipeline_group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
            pipeline_group.generalShader = VK_SHADER_UNUSED_KHR;
            pipeline_group.closestHitShader = 2;
            pipeline_group.anyHitShader = VK_SHADER_UNUSED_KHR;
            pipeline_group.intersectionShader = VK_SHADER_UNUSED_KHR;
            pipeline_group.pShaderGroupCaptureReplayHandle = nullptr;
            pipeline_groups.emplace_back(pipeline_group);

            store_resources(compiler, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
        }

        DescriptorSetLayouts set_layouts;
        set_layouts.reserve(pipeline_descriptor_layout.size());
        std::vector<VkDescriptorSetLayout> set_layout_handles;
        set_layout_handles.reserve(pipeline_descriptor_layout.size());
        for (const auto& [index, descriptors] : pipeline_descriptor_layout) {
            bool dynamic = false;
            std::uint32_t max_bindings = 0;
            const auto layout_hash = dtl::hash(0, descriptors);
            auto& layout = renderer.set_layout_cache[layout_hash];
            crd_unlikely_if(!layout) {
                std::vector<VkDescriptorBindingFlags> flags;
                flags.reserve(descriptors.size());
                std::vector<VkDescriptorSetLayoutBinding> bindings;
                bindings.reserve(descriptors.size());
                for (const auto& binding : descriptors) {
                    flags.emplace_back();
                    if (binding.dynamic) {
                        dynamic = true;
                        max_bindings = binding.count;
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
                layout_info.pNext = nullptr;
                if (context->extensions.descriptor_indexing) {
                    layout_info.pNext = &binding_flags;
                }
                layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                layout_info.flags = {};
                layout_info.bindingCount = bindings.size();
                layout_info.pBindings = bindings.data();
                crd_vulkan_check(vkCreateDescriptorSetLayout(context->device, &layout_info, nullptr, &layout));
            }
            set_layouts.push_back({ layout, max_bindings, dynamic });
            set_layout_handles.emplace_back(layout);
        }
        pipeline.type = Pipeline::type_raytracing;
        pipeline.layout.sets = std::move(set_layouts);
        pipeline.bindings = std::move(descriptor_layout_bindings);
        VkPipelineLayoutCreateInfo pipeline_layout_info;
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.pNext = nullptr;
        pipeline_layout_info.flags = {};
        pipeline_layout_info.setLayoutCount = set_layout_handles.size();
        pipeline_layout_info.pSetLayouts = set_layout_handles.data();
        pipeline_layout_info.pushConstantRangeCount = push_constant_range.size != 0;
        pipeline_layout_info.pPushConstantRanges = &push_constant_range;
        crd_vulkan_check(vkCreatePipelineLayout(context->device, &pipeline_layout_info, nullptr, &pipeline.layout.pipeline));

        VkPipelineDynamicStateCreateInfo pipeline_dynamic_states;
        pipeline_dynamic_states.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        pipeline_dynamic_states.pNext = nullptr;
        pipeline_dynamic_states.flags = {};
        pipeline_dynamic_states.dynamicStateCount = info.states.size();
        pipeline_dynamic_states.pDynamicStates = info.states.data();

        VkRayTracingPipelineCreateInfoKHR pipeline_info;
        pipeline_info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
        pipeline_info.pNext = nullptr;
        pipeline_info.flags = {};
        pipeline_info.stageCount = (std::uint32_t)pipeline_stages.size();
        pipeline_info.pStages = pipeline_stages.data();
        pipeline_info.groupCount = (std::uint32_t)pipeline_groups.size();
        pipeline_info.pGroups = pipeline_groups.data();
        pipeline_info.maxPipelineRayRecursionDepth = std::min(2u, context->gpu.raytracing_props.maxRayRecursionDepth);
        pipeline_info.pLibraryInfo = nullptr;
        pipeline_info.pLibraryInterface = nullptr;
        pipeline_info.pDynamicState = &pipeline_dynamic_states;
        pipeline_info.layout = pipeline.layout.pipeline;
        pipeline_info.basePipelineHandle = nullptr;
        pipeline_info.basePipelineIndex = 0;
        crd_vulkan_check(vkCreateRayTracingPipelinesKHR(context->device, nullptr, nullptr, 1, &pipeline_info, nullptr, &pipeline.handle));

        const auto& rt_props = context->gpu.raytracing_props;
        const auto handle_size = rt_props.shaderGroupHandleSize;
        const auto handle_size_aligned = aligned_size(handle_size, rt_props.shaderGroupHandleAlignment);

        std::vector<std::uint8_t> s_table_storage(pipeline_groups.size() * handle_size_aligned);
        crd_vulkan_check(vkGetRayTracingShaderGroupHandlesKHR(
            context->device,
            pipeline.handle,
            0,
            pipeline_groups.size(),
            s_table_storage.size(),
            s_table_storage.data()));

        const auto make_strided_region = [&](const StaticBuffer& buffer) {
            VkStridedDeviceAddressRegionKHR result;
            result.deviceAddress = buffer.address;
            result.stride = handle_size_aligned;
            result.size = handle_size_aligned;
            return result;
        };
        pipeline.sbt.raygen.storage = crd::make_static_buffer(*context, {
            .flags = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
            .usage = VMA_MEMORY_USAGE_CPU_ONLY,
            .capacity = handle_size
        });
        pipeline.sbt.raygen.region = make_strided_region(pipeline.sbt.raygen.storage);
        pipeline.sbt.raymiss.storage = crd::make_static_buffer(*context, {
            .flags = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
            .usage = VMA_MEMORY_USAGE_CPU_ONLY,
            .capacity = handle_size
        });
        pipeline.sbt.raymiss.region = make_strided_region(pipeline.sbt.raymiss.storage);
        pipeline.sbt.raychit.storage = crd::make_static_buffer(*context, {
            .flags = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
            .usage = VMA_MEMORY_USAGE_CPU_ONLY,
            .capacity = handle_size
        });
        pipeline.sbt.raychit.region = make_strided_region(pipeline.sbt.raychit.storage);

        // Copy handles
        std::memcpy(pipeline.sbt.raygen.storage.mapped, s_table_storage.data(), handle_size);
        std::memcpy(pipeline.sbt.raymiss.storage.mapped, s_table_storage.data() + handle_size_aligned, handle_size);
        std::memcpy(pipeline.sbt.raychit.storage.mapped, s_table_storage.data() + handle_size_aligned * 2, handle_size);

        return pipeline;
#else
        return {};
#endif
    }

    crd_module void Pipeline::destroy() noexcept {
        vkDestroyPipelineLayout(context->device, layout.pipeline, nullptr);
        vkDestroyPipeline(context->device, handle, nullptr);
        *this = {};
    }
} // namespace crd
