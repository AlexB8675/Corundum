#include <common.hpp>

struct LightVisibility {
    std::uint32_t count;
    std::uint32_t indices[max_lights_per_tile];
};

struct CameraUniform {
    glm::mat4 projection;
    glm::mat4 view;
    glm::vec3 position;
};

struct PointLightRadius {
    glm::vec4 position;
    glm::vec4 diffuse;
    glm::vec4 specular;
    glm::vec4 radius;
};

struct PointLightDraw {
    glm::vec4 color;
    glm::mat4 transform;
};

struct LightCullPC {
    glm::ivec2 viewport;
    glm::ivec2 tiles;
    std::uint32_t point_light_count;
};

static inline crd::GraphicsPipeline::CreateInfo depth_pipeline_info(const crd::RenderPass& pass) noexcept {
    return {
        .vertex = "../data/shaders/test_fwdp/depth.vert.spv",
        .geometry = nullptr,
        .fragment = nullptr,
        .render_pass = &pass,
        .attributes = {
            crd::vertex_attribute_vec3,
            crd::vertex_attribute_vec3,
            crd::vertex_attribute_vec2,
            crd::vertex_attribute_vec3,
            crd::vertex_attribute_vec3
        },
        .attachments = {},
        .states = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        },
        .cull = VK_CULL_MODE_BACK_BIT,
        .subpass = 0,
        .depth = {
            .test = true,
            .write = true
        }
    };
}

static inline crd::GraphicsPipeline::CreateInfo shadow_pipeline_info(const crd::RenderPass& pass) noexcept {
    return {
        .vertex = "../data/shaders/test_csm/shadow.vert.spv",
        .geometry = "../data/shaders/test_csm/shadow.geom.spv",
        .fragment = "../data/shaders/test_csm/shadow.frag.spv",
        .render_pass = &pass,
        .attributes = {
            crd::vertex_attribute_vec3,
            crd::vertex_attribute_vec3,
            crd::vertex_attribute_vec2,
            crd::vertex_attribute_vec3,
            crd::vertex_attribute_vec3
        },
        .attachments = {},
        .states = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        },
        .cull = VK_CULL_MODE_FRONT_BIT,
        .subpass = 0,
        .depth = {
            .test = true,
            .write = true
        }
    };
}

static inline crd::ComputePipeline::CreateInfo cull_pipeline_info() noexcept {
    return {
        .compute = "../data/shaders/test_fwdp/light_cull.comp.spv"
    };
}

static inline crd::GraphicsPipeline::CreateInfo light_pipeline_info(const crd::RenderPass& pass) noexcept {
    return {
        .vertex = "../data/shaders/test_fwdp/light.vert.spv",
        .geometry = nullptr,
        .fragment = "../data/shaders/test_fwdp/light.frag.spv",
        .render_pass = &pass,
        .attributes = {
            crd::vertex_attribute_vec3,
            crd::vertex_attribute_vec3,
            crd::vertex_attribute_vec2,
            crd::vertex_attribute_vec3,
            crd::vertex_attribute_vec3
        },
        .attachments = {
            crd::color_attachment_auto
        },
        .states = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        },
        .cull = VK_CULL_MODE_BACK_BIT,
        .subpass = 0,
        .depth = {
            .test = true,
            .write = true
        }
    };
}

static inline crd::GraphicsPipeline::CreateInfo final_pipeline_info(const crd::RenderPass& pass) noexcept {
    return {
        .vertex = "../data/shaders/test_fwdp/final.vert.spv",
        .geometry = nullptr,
        .fragment = "../data/shaders/test_fwdp/final.frag.spv",
        .render_pass = &pass,
        .attributes = {
            crd::vertex_attribute_vec3,
            crd::vertex_attribute_vec3,
            crd::vertex_attribute_vec2,
            crd::vertex_attribute_vec3,
            crd::vertex_attribute_vec3
        },
        .attachments = {
            crd::color_attachment_auto
        },
        .states = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        },
        .cull = VK_CULL_MODE_BACK_BIT,
        .subpass = 0,
        .depth = {
            .test = true,
            .write = false
        }
    };
}

int main() {
    auto window = crd::make_window(1280, 720, "Test FWDP");
    auto context = crd::make_context();
    auto renderer = crd::make_renderer(context);
    auto swapchain = crd::make_swapchain(context, window);
    auto depth_pass = crd::make_render_pass(context, {
        .attachments = { {
            .image = crd::make_image(context, {
                .width = window.width,
                .height = window.height,
                .mips = 1,
                .layers = 1,
                .format = VK_FORMAT_D32_SFLOAT,
                .aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                         VK_IMAGE_USAGE_SAMPLED_BIT
            }),
            .layout = {
                .initial = VK_IMAGE_LAYOUT_UNDEFINED,
                .final = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            },
            .clear = crd::make_clear_depth({ 1.0f, 0 }),
            .owning = true,
            .discard = false
        } },
        .subpasses = { {
            .attachments = { 0 },
            .preserve = {},
            .input = {}
        } },
        .dependencies = { {
            .source_subpass = crd::external_subpass,
            .dest_subpass = 0,
            .source_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dest_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                          VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .source_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dest_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
        }, {
            .source_subpass = 0,
            .dest_subpass = crd::external_subpass,
            .source_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dest_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                          VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            .source_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dest_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                           VK_ACCESS_SHADER_READ_BIT
        } },
        .framebuffers = { {
            { 0 }
        } }
    });
    auto shadow_pass = crd::make_render_pass(context, {
        .attachments = { {
            .image = crd::make_image(context, {
                .width   = 4096,
                .height  = 4096,
                .mips    = 1,
                .layers  = shadow_cascades,
                .format  = VK_FORMAT_D32_SFLOAT,
                .aspect  = VK_IMAGE_ASPECT_DEPTH_BIT,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .usage   = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                           VK_IMAGE_USAGE_SAMPLED_BIT
            }),
            .layout = {
                .initial = VK_IMAGE_LAYOUT_UNDEFINED,
                .final   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            },
            .clear   = crd::make_clear_depth({ 1.0f, 0 }),
            .owning  = true,
            .discard = false
        } },
        .subpasses = { {
            .attachments = { 0 },
            .preserve = {},
            .input = {}
        } },
        .dependencies = { {
            .source_subpass = crd::external_subpass,
            .dest_subpass = 0,
            .source_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dest_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .source_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dest_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
        }, {
            .source_subpass = 0,
            .dest_subpass = crd::external_subpass,
            .source_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dest_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .source_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dest_access = VK_ACCESS_SHADER_READ_BIT
        } },
        .framebuffers = { {
            .attachments = { 0 }
        } }
    });
    auto final_pass = crd::make_render_pass(context, {
        .attachments = { {
            .image = crd::make_image(context, {
                .width = window.width,
                .height = window.height,
                .mips = 1,
                .layers = 1,
                .format = swapchain.format,
                .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                         VK_IMAGE_USAGE_TRANSFER_SRC_BIT
            }),
            .layout = {
                .initial = VK_IMAGE_LAYOUT_UNDEFINED,
                .final = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
            },
            .clear = crd::make_clear_color({}),
            .owning = true,
            .discard = false
        }, {
            .image = depth_pass.image(0),
            .layout = {
                .initial = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .final = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            },
            .clear = {},
            .owning = false,
            .discard = false
        } },
        .subpasses = { {
            .attachments = { 0, 1 },
            .preserve = {},
            .input = {}
        } },
        .dependencies = { {
            .source_subpass = crd::external_subpass,
            .dest_subpass = 0,
            .source_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            .dest_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                          VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .source_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dest_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                           VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
        }, {
            .source_subpass = 0,
            .dest_subpass = crd::external_subpass,
            .source_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dest_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .source_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dest_access = VK_ACCESS_TRANSFER_READ_BIT
        } },
        .framebuffers = { {
            { 0, 1 }
        } }
    });
    auto shadow_pipeline = crd::make_pipeline(context, renderer, shadow_pipeline_info(shadow_pass));
    auto depth_pipeline = crd::make_pipeline(context, renderer, depth_pipeline_info(depth_pass));
    auto cull_pipeline = crd::make_pipeline(context, renderer, cull_pipeline_info());
    auto light_pipeline = crd::make_pipeline(context, renderer, light_pipeline_info(final_pass));
    auto final_pipeline = crd::make_pipeline(context, renderer, final_pipeline_info(final_pass));
    auto black = crd::request_static_texture(context, "../data/textures/black.png", crd::texture_srgb);
    std::vector<crd::Async<crd::StaticModel>> models;
    models.emplace_back(crd::request_static_model(context, "../data/models/cube/cube.obj"));
    models.emplace_back(crd::request_static_model(context, "../data/models/sponza/sponza.gltf"));
    models.emplace_back(crd::request_static_model(context, "../data/models/dragon/dragon.obj"));
    models.emplace_back(crd::request_static_model(context, "../data/models/suzanne/suzanne.obj"));
    models.emplace_back(crd::request_static_model(context, "../data/models/plane/plane.obj"));
    /*auto draw_cmds = std::vector<Draw>{ {
        .model = &models[1],
        .transforms = { {
            .position = glm::vec3(0.0f, -0.5f, 0.0f),
            .rotation = {},
            .scale = glm::vec3(0.02f)
        } }
    } };*/
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
            .position = glm::vec3(0.0),
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
    const auto p_lights = 256;
    std::vector<PointLightRadius> point_lights;
    point_lights.reserve(p_lights);
    for (int i = 0; i < p_lights; ++i) {
        const auto color = glm::vec4(random(0, 1), random(0, 1), random(0, 1), 0.0f);
        point_lights.push_back({
            .position = glm::vec4(random(-24, 24), random(0, 16), random(-12, 12), 0.0f),
            .diffuse = color,
            .specular = color,
            .radius = glm::vec4(10.0f),
        });
    }
    std::vector<PointLightDraw> point_light_instances;
    point_light_instances.reserve(p_lights);
    for (int i = 0; i < p_lights; ++i) {
        point_light_instances.push_back({
            point_lights[i].diffuse,
            glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(point_lights[i].position)), glm::vec3(0.1f))
        });
    }
    DirectionalLight sun_dlight;
    sun_dlight.direction = {};
    sun_dlight.diffuse = glm::vec4(1.0f);
    sun_dlight.specular = glm::vec4(1.0f);
    auto camera_buffer = crd::make_buffer(context, {
        .type = crd::uniform_buffer,
        .usage = crd::host_visible,
        .capacity = sizeof(CameraUniform)
    });
    auto model_buffer = crd::make_buffer(context, {
        .type = crd::storage_buffer,
        .usage = crd::host_visible,
        .capacity = sizeof(glm::mat4)
    });
    auto cascades_buffer = crd::make_buffer(context, {
        .type = crd::uniform_buffer,
        .usage = crd::host_visible,
        .capacity = sizeof(Cascade[shadow_cascades])
    });
    auto light_instances_buffer = crd::make_buffer(context, {
        .type = crd::storage_buffer,
        .usage = crd::host_visible,
        .capacity = crd::size_bytes(point_light_instances)
    });
    auto point_lights_buffer = crd::make_buffer(context, {
        .type = crd::storage_buffer,
        .usage = crd::host_visible,
        .capacity = crd::size_bytes(point_lights)
    });
    auto directional_lights_buffer = crd::make_buffer(context, {
        .type = crd::uniform_buffer,
        .usage = crd::host_visible,
        .capacity = sizeof(DirectionalLight[max_directional_lights])
    });
    auto light_visibility_buffer = crd::make_buffer(context, {
        .type = crd::storage_buffer,
        .usage = crd::device_local,
        .capacity = sizeof(LightVisibility)
    });
    auto depth_set = crd::make_descriptor_set(context, depth_pipeline.layout.sets[0]);
    auto shadow_set = crd::make_descriptor_set(context, shadow_pipeline.layout.sets[0]);
    auto cmp_cull_set = crd::make_descriptor_set(context, cull_pipeline.layout.sets[0]);
    auto main_set = crd::make_descriptor_set(context, final_pipeline.layout.sets[0]);
    auto light_data_set = crd::make_descriptor_set(context, final_pipeline.layout.sets[1]);
    auto light_view_set = crd::make_descriptor_set(context, light_pipeline.layout.sets[0]);
    window.set_resize_callback([&]() {
        depth_pass.resize(context, {
            .size = { swapchain.width, swapchain.height },
            .framebuffer = 0,
            .attachments = { 0 }
        });
        final_pass.resize(context, {
            .size = { swapchain.width, swapchain.height },
            .framebuffer = 0,
            .attachments = { 0 },
            .references = { &depth_pass.attachments[0] }
        });
    });
    window.set_key_callback([&](crd::Key key, crd::KeyState state) {
        switch (key) {
            case crd::key_r: {
                crd_unlikely_if(state == crd::key_pressed) {
                    depth_pipeline = reload_pipelines(context, renderer, depth_pipeline, depth_pipeline_info(depth_pass));
                    cull_pipeline = reload_pipelines(context, renderer, cull_pipeline, cull_pipeline_info());
                    final_pipeline = reload_pipelines(context, renderer, final_pipeline, final_pipeline_info(final_pass));
                    light_pipeline = reload_pipelines(context, renderer, light_pipeline, light_pipeline_info(final_pass));
                }
            } break;
        }
    });
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
        const auto scene = build_scene(draw_cmds, black->info());
        CameraUniform camera_data;
        camera_data.projection = camera.projection;
        camera_data.view = camera.view;
        camera_data.position = camera.position;

        crd::wait_fence(context, done);

        sun_dlight.direction = glm::vec4(
            50.0f * std::cos(crd::current_time() / 6),
            150.0f,
            50.0f * std::sin(crd::current_time() / 6),
            1.0f);
        const auto cascades = calculate_cascades(camera, sun_dlight.direction);
        const auto tiles_per_row = (window.width - 1) / tile_size + 1;
        const auto tiles_per_col = (window.height - 1) / tile_size + 1;
        const auto light_visibility_size = sizeof(LightVisibility) * tiles_per_col * tiles_per_row;
        crd::resize_buffer(context, model_buffer[index], crd::size_bytes(scene.transforms));
        crd::resize_buffer(context, light_visibility_buffer[index], light_visibility_size);
        crd::resize_buffer(context, point_lights_buffer[index], crd::size_bytes(point_lights));

        camera_buffer[index].write(&camera_data, 0, sizeof(CameraUniform));
        cascades_buffer[index].write(cascades.data(), 0);
        point_lights_buffer[index].write(point_lights.data(), 0, crd::size_bytes(point_lights));
        directional_lights_buffer[index].write(&sun_dlight, 0, sizeof sun_dlight);
        light_instances_buffer[index].write(point_light_instances.data(), 0, crd::size_bytes(point_light_instances));
        model_buffer[index].write(scene.transforms.data(), 0, crd::size_bytes(scene.transforms));

        depth_set[index]
            .bind(context, depth_pipeline.bindings["Uniforms"], camera_buffer[index].info())
            .bind(context, depth_pipeline.bindings["Models"], model_buffer[index].info());
        shadow_set[index]
            .bind(context, shadow_pipeline.bindings["Models"], model_buffer[index].info())
            .bind(context, shadow_pipeline.bindings["Cascades"], cascades_buffer[index].info())
            .bind(context, shadow_pipeline.bindings["textures"], scene.descriptors);
        cmp_cull_set[index]
            .bind(context, cull_pipeline.bindings["CameraBuffer"], camera_buffer[index].info())
            .bind(context, cull_pipeline.bindings["PointLights"], point_lights_buffer[index].info())
            .bind(context, cull_pipeline.bindings["depth"], depth_pass.image(0).sample(context.default_sampler))
            .bind(context, cull_pipeline.bindings["LightVisibilities"], light_visibility_buffer[index].info());
        main_set[index]
            .bind(context, final_pipeline.bindings["Uniforms"], camera_buffer[index].info())
            .bind(context, final_pipeline.bindings["Models"], model_buffer[index].info())
            .bind(context, final_pipeline.bindings["textures"], scene.descriptors);
        light_data_set[index]
            .bind(context, final_pipeline.bindings["PointLights"], point_lights_buffer[index].info())
            .bind(context, final_pipeline.bindings["DirectionalLights"], directional_lights_buffer[index].info())
            .bind(context, final_pipeline.bindings["LightVisibilities"], light_visibility_buffer[index].info())
            .bind(context, final_pipeline.bindings["Cascades"], cascades_buffer[index].info())
            .bind(context, final_pipeline.bindings["shadow"], shadow_pass.image(0).sample(context.shadow_sampler));
        light_view_set[index]
            .bind(context, light_pipeline.bindings["Uniforms"], camera_buffer[index].info())
            .bind(context, light_pipeline.bindings["Instances"], light_instances_buffer[index].info());

        commands
            .begin()
            .begin_render_pass(depth_pass, 0)
            .bind_pipeline(depth_pipeline)
            .bind_descriptor_set(0, depth_set[index])
            .set_viewport()
            .set_scissor();
        for (const auto& model : scene.models) {
            auto& raw_model = **model.handle;
            for (const auto& submesh : model.submeshes) {
                auto& raw_submesh = raw_model.submeshes[submesh.index];
                const std::uint32_t indices[] = {
                    model.transform
                };
                commands
                    .push_constants(VK_SHADER_STAGE_VERTEX_BIT, indices, sizeof indices)
                    .bind_static_mesh(*raw_submesh.mesh)
                    .draw_indexed(raw_submesh.indices, model.instances, 0, 0, 0);
            }
        }
        commands
            .end_render_pass()
            .begin_render_pass(shadow_pass, 0)
            .bind_pipeline(shadow_pipeline)
            .bind_descriptor_set(0, shadow_set[index])
            .set_viewport(crd::inverted_viewport)
            .set_scissor();
        for (const auto& model : scene.models) {
            auto& raw_model = **model.handle;
            for (const auto& submesh : model.submeshes) {
                auto& raw_submesh = raw_model.submeshes[submesh.index];
                const std::uint32_t indices[] = {
                    model.transform,
                    submesh.textures[0]
                };
                commands
                    .push_constants(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, indices, sizeof indices)
                    .bind_static_mesh(*raw_submesh.mesh)
                    .draw_indexed(raw_submesh.indices, model.instances, 0, 0, 0);
            }
        }
        LightCullPC cull_constants;
        cull_constants.viewport = { window.width, window.height };
        cull_constants.tiles = { tiles_per_row, tiles_per_col };
        cull_constants.point_light_count = point_lights.size();
        commands
            .end_render_pass()
            .bind_pipeline(cull_pipeline)
            .bind_descriptor_set(0, cmp_cull_set[index])
            .push_constants(VK_SHADER_STAGE_COMPUTE_BIT, &cull_constants, sizeof cull_constants)
            .dispatch(tiles_per_row, tiles_per_col)
            .barrier({
                .buffer = &light_visibility_buffer[index].handle,
                .source_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .dest_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .source_access = VK_ACCESS_SHADER_WRITE_BIT,
                .dest_access = VK_ACCESS_SHADER_READ_BIT,
            })
            .begin_render_pass(final_pass, 0)
            .bind_pipeline(final_pipeline)
            .set_viewport()
            .set_scissor()
            .bind_descriptor_set(0, main_set[index])
            .bind_descriptor_set(1, light_data_set[index]);
        for (const auto& model : scene.models) {
            auto& raw_model = **model.handle;
            for (const auto& submesh : model.submeshes) {
                auto& raw_submesh = raw_model.submeshes[submesh.index];
                const std::uint32_t indices[] = {
                    model.transform,
                    submesh.textures[0],
                    submesh.textures[1],
                    submesh.textures[2],
                    1,
                    0, // Padding
                    tiles_per_row,
                    tiles_per_col
                };
                commands
                    .push_constants(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, indices, sizeof indices)
                    .bind_static_mesh(*raw_submesh.mesh)
                    .draw_indexed(raw_submesh.indices, model.instances, 0, 0, 0);
            }
        }
        //auto& cube_mesh = models[0]->submeshes[0];
        commands
            //.bind_pipeline(light_pipeline)
            //.bind_descriptor_set(0, light_view_set[index])
            //.bind_static_mesh(*cube_mesh.mesh)
            //.draw_indexed(cube_mesh.indices, p_lights, 0, 0, 0)
            .end_render_pass()
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
            .copy_image(final_pass.image(0), image)
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
            .end();
        crd::present_frame(context, renderer, {
            .commands = commands,
            .window = window,
            .swapchain = swapchain,
            .waits = {},
            .stages = { VK_PIPELINE_STAGE_TRANSFER_BIT }
        });
        if (fps >= 2) {
            crd::log("Scene", crd::severity_info, crd::type_performance, "Average FPS: %lf, DT: %lfms", 1 / (fps / frames), (fps / frames) * 1000);
            frames = 0;
            fps = 0;
        }
    }
    return 0;
}