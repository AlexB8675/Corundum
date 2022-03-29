#include <common.hpp>

struct CameraUniform {
    glm::mat4 projection;
    glm::mat4 view;
};

struct ObjectAddress {
    std::uint32_t diffuse_index;
    std::uint32_t normal_index;
    std::uint32_t specular_index;
    VkDeviceAddress vertex_address;
    VkDeviceAddress index_address;
};

struct RTScene {
    crd::in_flight_array<crd::TopLevelAS> tlas;
    crd::in_flight_array<std::size_t> cache = {
        (std::size_t)-1,
        (std::size_t)-1
    };
    std::vector<VkDescriptorImageInfo> descriptors;
    crd::Buffer<crd::in_flight> objects;
};

static inline VkTransformMatrixKHR as_vulkan(glm::mat4 matrix) noexcept {
    VkTransformMatrixKHR result;
    matrix = glm::transpose(matrix);
    std::memcpy(&result, &matrix, sizeof result);
    return result;
}

static inline void make_scene(const crd::Context& context, VkDescriptorImageInfo fallback, RTScene& scene, crd::CommandBuffer& commands, std::span<Draw> draw_cmds, std::uint32_t index) noexcept {
    auto& tlas = scene.tlas[index];
    auto& object = scene.objects[index];
    scene.descriptors = { fallback };
    std::unordered_map<void*, std::uint32_t> cache;
    std::vector<ObjectAddress> object_addresses;
    std::vector<VkAccelerationStructureInstanceKHR> instances;
    cache.reserve(512);
    for (const auto& draw_cmd : draw_cmds) {
        auto& model = *draw_cmd.model;
        crd_likely_if(model.is_ready()) {
            const auto data_grow = draw_cmd.transforms.size() * model->submeshes.size();
            instances.reserve(instances.size() + data_grow);
            object_addresses.reserve(object_addresses.size() + data_grow);
            for (auto& submesh : model->submeshes) {
                crd_likely_if(submesh.mesh.is_ready()) {
                    const auto emplace_descriptor = [&](crd::Async<crd::StaticTexture>* texture) -> std::uint32_t {
                        crd_likely_if(texture) {
                            auto& cached = cache[texture];
                            crd_unlikely_if(cached == 0 && texture->is_ready()) {
                                scene.descriptors.emplace_back((*texture)->info());
                                cached = scene.descriptors.size() - 1;
                            }
                            return cached;
                        }
                        return 0;
                    };
                    VkAccelerationStructureInstanceKHR as_instance;
                    as_instance.mask = 0xff;
                    as_instance.instanceShaderBindingTableRecordOffset = 0;
                    as_instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
                    as_instance.accelerationStructureReference = submesh.mesh->blas.address;
                    for (const auto& transform : draw_cmd.transforms) {
                        auto result = glm::mat4(1.0f);
                        result = glm::translate(result, transform.position);
                        crd_unlikely_if(std::fpclassify(transform.rotation.angle) != FP_ZERO) {
                            result = glm::rotate(result, transform.rotation.angle, transform.rotation.axis);
                        }
                        result = glm::scale(result, transform.scale);
                        as_instance.transform = as_vulkan(result);
                        as_instance.instanceCustomIndex = instances.size();
                        instances.emplace_back(as_instance);
                        object_addresses.push_back({
                            emplace_descriptor(submesh.diffuse),
                            emplace_descriptor(submesh.normal),
                            emplace_descriptor(submesh.specular),
                            submesh.mesh->geometry.address,
                            submesh.mesh->indices.address
                        });
                    }
                }
            }
        }
    }

    const auto obj_addresses_capacity = std::max(crd::size_bytes(object_addresses), sizeof(ObjectAddress));
    crd_unlikely_if(object.size == 0) {
        object = crd::make_buffer<1>(context, {
            .type = crd::storage_buffer,
            .usage = crd::host_visible,
            .capacity = obj_addresses_capacity
        });
    } else {
        crd::resize_buffer(context, object, obj_addresses_capacity);
    }
    object.write(object_addresses.data(), 0);

    const auto instances_capacity = std::max(crd::size_bytes(instances), sizeof(VkAccelerationStructureInstanceKHR));
    crd_unlikely_if(tlas.instances.handle && tlas.instances.capacity < instances_capacity) {
        crd::destroy_static_buffer(context, tlas.instances);
    }
    crd_unlikely_if(!tlas.instances.handle) {
        tlas.instances = crd::make_static_buffer(context, {
            .flags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
            .capacity = instances_capacity
        });
    }
    crd_unlikely_if(!instances.empty()) {
        std::memcpy(tlas.instances.mapped, instances.data(), instances_capacity);
    }

    VkAccelerationStructureGeometryKHR as_geometry_info = {};
    as_geometry_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    as_geometry_info.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    as_geometry_info.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    as_geometry_info.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    as_geometry_info.geometry.instances.arrayOfPointers = false;
    as_geometry_info.geometry.instances.data.deviceAddress = tlas.instances.address;

    VkAccelerationStructureBuildGeometryInfoKHR as_build_geometry_info = {};
    as_build_geometry_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    as_build_geometry_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    as_build_geometry_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    as_build_geometry_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    as_build_geometry_info.geometryCount = 1;
    as_build_geometry_info.pGeometries = &as_geometry_info;

    std::uint32_t primitive_count = instances.size();
    VkAccelerationStructureBuildSizesInfoKHR as_build_sizes_info = {};
    as_build_sizes_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    crd::vkGetAccelerationStructureBuildSizesKHR(
        context.device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &as_build_geometry_info,
        &primitive_count,
        &as_build_sizes_info);

    crd_unlikely_if(scene.cache[index] != primitive_count) {
        crd::dtl::log("Vulkan", crd::dtl::severity_info, crd::dtl::type_general, "creating TLAS, requesting: %llu bytes", as_build_sizes_info.accelerationStructureSize);
        crd_likely_if(tlas.handle) {
            crd::vkDestroyAccelerationStructureKHR(context.device, tlas.handle, nullptr);
        }
        crd_likely_if(tlas.buffer.handle) {
            crd::destroy_static_buffer(context, tlas.buffer);
        }
        tlas.buffer = crd::make_static_buffer(context, {
            .flags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .capacity = as_build_sizes_info.accelerationStructureSize
        });
        VkAccelerationStructureCreateInfoKHR as_info = {};
        as_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        as_info.createFlags = {};
        as_info.buffer = tlas.buffer.handle;
        as_info.offset = 0;
        as_info.size = tlas.buffer.capacity;
        as_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        as_info.deviceAddress = 0;
        crd_vulkan_check(crd::vkCreateAccelerationStructureKHR(context.device, &as_info, nullptr, &tlas.handle));
        tlas.address = crd::device_address(context, tlas);
    }
    scene.cache[index] = primitive_count;

    crd_likely_if(tlas.build.handle) {
        crd::destroy_static_buffer(context, tlas.build);
    }
    tlas.build = crd::make_static_buffer(context, {
        .flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .capacity = as_build_sizes_info.buildScratchSize
    });
    as_build_geometry_info.dstAccelerationStructure = tlas.handle;
    as_build_geometry_info.scratchData.deviceAddress = tlas.build.address;
    VkAccelerationStructureBuildRangeInfoKHR as_build_range_info;
    as_build_range_info.primitiveCount = primitive_count;
    as_build_range_info.primitiveOffset = 0;
    as_build_range_info.firstVertex = 0;
    as_build_range_info.transformOffset = 0;
    commands
        .build_acceleration_structure(&as_build_geometry_info, &as_build_range_info)
        .memory_barrier({
            .source_stage = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            .dest_stage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            .source_access = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
            .dest_access = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
        });
}

int main() {
    auto window = crd::make_window(1280, 720, "Test RT");
    auto context = crd::make_context();
    auto renderer = crd::make_renderer(context);
    auto swapchain = crd::make_swapchain(context, window);
    auto black = crd::request_static_texture(context, "../data/textures/black.png", crd::texture_srgb);
    std::vector<crd::Async<crd::StaticModel>> models;
    models.emplace_back(crd::request_static_model(context, "../data/models/cube/cube.obj"));
    models.emplace_back(crd::request_static_model(context, "../data/models/sponza/sponza.gltf"));
    models.emplace_back(crd::request_static_model(context, "../data/models/dragon/dragon.obj"));
    models.emplace_back(crd::request_static_model(context, "../data/models/suzanne/suzanne.obj"));
    auto draw_cmds = std::to_array<Draw>({ {
        .model = &models[0],
        .transforms = { {
            .position = glm::vec3(0.0f, 1.5f, 0.0f),
            .rotation = {},
            .scale = glm::vec3(0.5f)
        }, {
            .position = glm::vec3(2.0f, 0.0f, 1.0f),
            .rotation = {},
            .scale = glm::vec3(0.5f)
        }, {
            .position = glm::vec3(-1.0f, 0.0f, 2.0f),
            .rotation = {
                .axis = glm::normalize(glm::vec3(1.0f, 0.0f, 1.0f)),
                .angle = glm::radians(45.0f)
            },
            .scale = glm::vec3(0.25f)
        } }
    }, {
        .model = &models[1],
        .transforms = { {
            .position = glm::vec3(0.0f, -0.5f, 0.0f),
            .rotation = {},
            .scale = glm::vec3(0.02f)
        } }
    }, {
        .model = &models[2],
        .transforms = { {
            .position = glm::vec3(-4.0f, 1.0f, 0.0f),
            .rotation = {},
            .scale = glm::vec3(2.0f)
        } }
    }, {
        .model = &models[3],
        .transforms = { {
            .position = glm::vec3(4.0f, 1.0f, 0.0f),
            .rotation = {},
            .scale = glm::vec3(0.5f)
        } }
    } });

    auto result = crd::make_image(context, {
        .width   = window.width,
        .height  = window.height,
        .mips    = 1,
        .layers  = 1,
        .format  = VK_FORMAT_B8G8R8A8_UNORM,
        .aspect  = VK_IMAGE_ASPECT_COLOR_BIT,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .usage   = VK_IMAGE_USAGE_STORAGE_BIT |
                   VK_IMAGE_USAGE_TRANSFER_SRC_BIT
    });

    // Transition
    {
        auto commands = crd::make_command_buffer(context, {
            .pool = context.graphics->pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY
        });
        commands
            .begin()
            .transition_layout({
                .image = &result,
                .mip = 0,
                .level = 0,
                .source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                .dest_stage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                .source_access = {},
                .dest_access = VK_ACCESS_SHADER_WRITE_BIT,
                .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                .new_layout = VK_IMAGE_LAYOUT_GENERAL
            })
            .end();
        crd::immediate_submit(context, commands, crd::queue_type_graphics);
    }

    auto main_pipeline = crd::make_pipeline(context, renderer, {
        .raygen = "../data/shaders/test_raytracing/main.rgen.spv",
        .raymiss = "../data/shaders/test_raytracing/main.rmiss.spv",
        .raychit = "../data/shaders/test_raytracing/main.rchit.spv",
        .states = {}
    });

    auto camera_buffer = crd::make_buffer(context, {
        .type = crd::uniform_buffer,
        .usage = crd::host_visible,
        .capacity = sizeof(CameraUniform)
    });

    auto main_set = crd::make_descriptor_set(context, main_pipeline.layout.sets[0]);

    RTScene scene = {};
    Camera camera;
    std::size_t frames = 0;
    double last_time = 0;
    double fps = 0;
    while (!window.is_closed()) {
        crd::poll_events();
        auto [commands, image, index, wait, signal, done] = crd::acquire_frame(context, renderer, window, swapchain);
        const auto current_time = crd::current_time();
        const auto delta_time = current_time - last_time;
        last_time = current_time;
        fps += delta_time;
        ++frames;
        camera.update(window, delta_time);
        CameraUniform camera_data;
        camera_data.projection = camera.projection;
        camera_data.projection[1][1] *= -1;
        camera_data.projection = glm::inverse(camera_data.projection);
        camera_data.view = glm::inverse(camera.view);

        crd::wait_fence(context, done);

        camera_buffer.write(&camera_data, 0, sizeof camera_data);

        make_scene(context, black->info(), scene, commands.begin(), draw_cmds, index);

        main_set[index]
            .bind(context, main_pipeline.bindings["Camera"], camera_buffer[index].info())
            .bind(context, main_pipeline.bindings["image"], result.info(VK_IMAGE_LAYOUT_GENERAL))
            .bind(context, main_pipeline.bindings["tlas"], scene.tlas[index])
            .bind(context, main_pipeline.bindings["Objects"], scene.objects[index].info())
            .bind(context, main_pipeline.bindings["textures"], scene.descriptors);

        commands
            .bind_pipeline(main_pipeline)
            .bind_descriptor_set(0, main_set[index])
            .trace_rays(window.width, window.height)
            .transition_layout({
                .image = &result,
                .mip = 0,
                .level = 0,
                .source_stage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                .dest_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .source_access = VK_ACCESS_SHADER_WRITE_BIT,
                .dest_access = VK_ACCESS_TRANSFER_READ_BIT,
                .old_layout = VK_IMAGE_LAYOUT_GENERAL,
                .new_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
            })
            .transition_layout({
                .image = &image,
                .mip = 0,
                .level = 0,
                .source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                .dest_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .source_access = {},
                .dest_access = VK_ACCESS_TRANSFER_WRITE_BIT,
                .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                .new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            })
            .copy_image(result, image)
            .transition_layout({
                .image = &image,
                .mip = 0,
                .level = 0,
                .source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .dest_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                .source_access = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dest_access = {},
                .old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .new_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
            })
            .transition_layout({
                .image = &result,
                .mip = 0,
                .level = 0,
                .source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .dest_stage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                .source_access = {},
                .dest_access = VK_ACCESS_SHADER_WRITE_BIT,
                .old_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .new_layout = VK_IMAGE_LAYOUT_GENERAL
            })
            .end();
        crd::present_frame(context, renderer, {
            .commands = commands,
            .window = window,
            .swapchain = swapchain,
            .waits = {},
            .stages = { VK_PIPELINE_STAGE_TRANSFER_BIT }
        });
        if (fps >= 1.6) {
            crd::dtl::log("Scene", crd::dtl::severity_info, crd::dtl::type_performance, "Average FPS: %lf ", 1 / (fps / frames));
            frames = 0;
            fps = 0;
        }
    }
}