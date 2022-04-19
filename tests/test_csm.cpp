#include <common.hpp>

struct CameraUniform {
    glm::mat4 projection;
    glm::mat4 view;
    glm::vec4 position;
};

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

static inline crd::GraphicsPipeline::CreateInfo deferred_pipeline_info(const crd::RenderPass& pass) noexcept {
    return {
        .vertex = "../data/shaders/test_csm/gbuffer.vert.spv",
        .geometry = nullptr,
        .fragment = "../data/shaders/test_csm/gbuffer.frag.spv",
        .render_pass = &pass,
        .attributes = {
            crd::vertex_attribute_vec3,
            crd::vertex_attribute_vec3,
            crd::vertex_attribute_vec2,
            crd::vertex_attribute_vec3,
            crd::vertex_attribute_vec3
        },
        .attachments = {
            crd::color_attachment_disable_blend,
            crd::color_attachment_disable_blend,
            crd::color_attachment_disable_blend,
            crd::color_attachment_disable_blend
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

static inline crd::GraphicsPipeline::CreateInfo main_pipeline_info(const crd::RenderPass& pass) noexcept {
    return {
        .vertex = "../data/shaders/test_csm/main.vert.spv",
        .geometry = nullptr,
        .fragment = "../data/shaders/test_csm/main.frag.spv",
        .render_pass = &pass,
        .attributes = {},
        .attachments = {
            crd::color_attachment_auto
        },
        .states = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        },
        .cull = VK_CULL_MODE_NONE,
        .subpass = 1,
        .depth = {
            .test = false,
            .write = false
        }
    };
}

int main() {
    auto window = crd::make_window(1280, 720, "Cascaded Shadows Test");
    auto context = crd::make_context();
    auto renderer = crd::make_renderer(context);
    auto swapchain = crd::make_swapchain(context, window);
    auto deferred_pass = crd::make_render_pass(context, {
        .attachments = { {
            .image = crd::make_image(context, { // Final Color.
                .width   = window.width,
                .height  = window.height,
                .mips    = 1,
                .layers  = 1,
                .format  = swapchain.format,
                .aspect  = VK_IMAGE_ASPECT_COLOR_BIT,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .usage   = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT
            }),
            .layout = {
                .initial = VK_IMAGE_LAYOUT_UNDEFINED,
                .final   = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
            },
            .clear   = crd::make_clear_color({
                24  / 255.0f,
                154 / 255.0f,
                207 / 255.0f,
                1.0f
            }),
            .owning  = true,
            .discard = false
        }, {
            .image = crd::make_image(context, { // Position.
                .width   = window.width,
                .height  = window.height,
                .mips    = 1,
                .layers  = 1,
                .format  = VK_FORMAT_R16G16B16A16_SFLOAT,
                .aspect  = VK_IMAGE_ASPECT_COLOR_BIT,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .usage   = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                           VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
            }),
            .layout = {
                .initial = VK_IMAGE_LAYOUT_UNDEFINED,
                .final   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            },
            .clear   = crd::make_clear_color({}),
            .owning  = true,
            .discard = false
        }, {
            .image = crd::make_image(context, { // Normal.
                .width   = window.width,
                .height  = window.height,
                .mips    = 1,
                .layers  = 1,
                .format  = VK_FORMAT_R16G16B16A16_SFLOAT,
                .aspect  = VK_IMAGE_ASPECT_COLOR_BIT,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .usage   = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                           VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
            }),
            .layout = {
                .initial = VK_IMAGE_LAYOUT_UNDEFINED,
                .final   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            },
            .clear   = crd::make_clear_color({}),
            .owning  = true,
            .discard = false
        }, {
            .image = crd::make_image(context, { // Specular.
                .width   = window.width,
                .height  = window.height,
                .mips    = 1,
                .layers  = 1,
                .format  = VK_FORMAT_R8G8B8A8_UNORM,
                .aspect  = VK_IMAGE_ASPECT_COLOR_BIT,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .usage   = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                           VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
            }),
            .layout = {
                .initial = VK_IMAGE_LAYOUT_UNDEFINED,
                .final   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            },
            .clear   = crd::make_clear_color({}),
            .owning  = true,
            .discard = false
        }, {
            .image = crd::make_image(context, { // Albedo.
                .width   = window.width,
                .height  = window.height,
                .mips    = 1,
                .layers  = 1,
                .format  = VK_FORMAT_R8G8B8A8_SRGB,
                .aspect  = VK_IMAGE_ASPECT_COLOR_BIT,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .usage   = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                           VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
            }),
            .layout = {
                .initial = VK_IMAGE_LAYOUT_UNDEFINED,
                .final   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            },
            .clear   = crd::make_clear_color({}),
            .owning  = true,
            .discard = false
        }, {
            .image = crd::make_image(context, { // Depth.
                .width   = window.width,
                .height  = window.height,
                .mips    = 1,
                .layers  = 1,
                .format  = VK_FORMAT_D32_SFLOAT,
                .aspect  = VK_IMAGE_ASPECT_DEPTH_BIT,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .usage   = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
            }),
            .layout = {
                .initial = VK_IMAGE_LAYOUT_UNDEFINED,
                .final   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            },
            .clear   = crd::make_clear_depth({ 1.0f, 0 }),
            .owning  = true,
            .discard = false
        } },
        .subpasses = { {
            .attachments = { 1, 2, 3, 4, 5 },
            .preserve    = {},
            .input       = {}
        }, {
            .attachments = { 0, 5 },
            .preserve    = {},
            .input       = { 1, 2, 3, 4 }
        } },
        .dependencies = { {
            .source_subpass = crd::external_subpass,
            .dest_subpass   = 0,
            .source_stage   = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dest_stage     = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .source_access  = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                              VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dest_access    = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                              VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
        }, {
            .source_subpass = 0,
            .dest_subpass   = 1,
            .source_stage   = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dest_stage     = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .source_access  = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                              VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dest_access    = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT
        }, {
            .source_subpass = 1,
            .dest_subpass   = crd::external_subpass,
            .source_stage   = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dest_stage     = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .source_access  = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dest_access    = VK_ACCESS_TRANSFER_READ_BIT
        } },
        .framebuffers = {
            { { 0, 1, 2, 3, 4, 5 } }
        }
    });
    auto shadow_pass = crd::make_render_pass(context, {
        .attachments = { {
            .image = crd::make_image(context, {
                .width   = 2048,
                .height  = 2048,
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
    auto shadow_pipeline = crd::make_pipeline(context, renderer, shadow_pipeline_info(shadow_pass));
    auto deferred_pipeline = crd::make_pipeline(context, renderer, deferred_pipeline_info(deferred_pass));
    auto main_pipeline = crd::make_pipeline(context, renderer, main_pipeline_info(deferred_pass));
    window.resize_callback = [&]() {
        deferred_pass.resize(context, {
            .size = { swapchain.width, swapchain.height },
            .framebuffer = 0,
            .attachments = { 0, 1, 2, 3, 4, 5 }
        });
    };
    window.set_key_callback([&](crd::Key key, crd::KeyState state) {
        switch (key) {
            case crd::key_f: {
                if (state == crd::key_pressed) {
                    window.toggle_fullscreen();
                }
            } break;

            case crd::key_r: {
                crd_unlikely_if(state == crd::key_pressed) {
                    shadow_pipeline = reload_pipelines(context, renderer, shadow_pipeline, shadow_pipeline_info(shadow_pass));
                    deferred_pipeline = reload_pipelines(context, renderer, deferred_pipeline, deferred_pipeline_info(deferred_pass));
                    main_pipeline = reload_pipelines(context, renderer, main_pipeline, main_pipeline_info(deferred_pass));
                }
            } break;

            case crd::key_esc: {
                if (state == crd::key_pressed) {
                    window.close();
                }
            } break;
        }
    });
    auto black = crd::request_static_texture(context, "../data/textures/black.png", crd::texture_srgb);
    auto models = std::vector<crd::Async<crd::StaticModel>>();
    models.emplace_back(crd::request_static_model(context, "../data/models/cube/cube.obj"));
    models.emplace_back(crd::request_static_model(context, "../data/models/sponza/sponza.gltf"));
    models.emplace_back(crd::request_static_model(context, "../data/models/dragon/dragon.obj"));
    models.emplace_back(crd::request_static_model(context, "../data/models/suzanne/suzanne.obj"));
    //models.emplace_back(crd::request_static_model(context, "../data/models/deccer-cubes/SM_Deccer_Cubes_Textured.obj"));
    //models.emplace_back(crd::request_static_model(context, "../data/models/rungholt/rungholt.obj"));
    //models.emplace_back(crd::request_static_model(context, "../data/models/rungholt/house.obj"));
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
    /*auto draw_cmds = std::vector<Draw>({ {
        .model = &models[0],
        .transforms = { {
            .position = glm::vec3(0.0f, 0.0f, 0.0f),
            .rotation = {},
            .scale = glm::vec3(0.35f)
        } }
    }, {
        .model = &models[1],
        .transforms = { {
            .position = glm::vec3(0.0f, 0.0f, 0.0f),
            .rotation = {},
            .scale = glm::vec3(0.35f)
        } }
    } });*/
    std::vector<PointLight> point_lights;
    //point_lights.reserve(4);
    //for (int i = 0; i < 4; ++i) {
    //    point_lights.push_back({
    //        .position = glm::vec4(random(-20, 20), random(4, 16), random(-16, 16), 0.0f),
    //        .diffuse = glm::vec4(random(0, 1), random(0, 1), random(0, 1), 1.0f),
    //        .specular = glm::vec4(1.0f),
    //        .falloff = glm::vec4(1.0f, 0.125f, 0.075f, 1.0f)
    //    });
    //}
    std::vector<DirectionalLight> dir_lights = { {
        .direction = glm::vec4(1.0f),
        .diffuse = glm::vec4(1.0f),
        .specular = glm::vec4(1.0f)
    } };
    Camera camera;
    auto cascades_buffer = crd::make_buffer(context, {
        .type = crd::uniform_buffer,
        .usage = crd::host_visible,
        .capacity = sizeof(Cascade[max_shadow_cascades]),
    });
    auto camera_buffer = crd::make_buffer(context, {
        .type = crd::uniform_buffer,
        .usage = crd::host_visible,
        .capacity = sizeof(CameraUniform),
    });
    auto model_buffer = crd::make_buffer(context, {
        .type = crd::storage_buffer,
        .usage = crd::host_visible,
        .capacity = sizeof(glm::mat4),
    });
    auto point_light_buffer = crd::make_buffer(context, {
        .type = crd::storage_buffer,
        .usage = crd::host_visible,
        .capacity = sizeof(glm::mat4),
    });
    auto directional_light_buffer = crd::make_buffer(context, {
        .type = crd::uniform_buffer,
        .usage = crd::host_visible,
        .capacity = crd::size_bytes(dir_lights),
    });

    auto shadow_set = crd::make_descriptor_set(context, shadow_pipeline.layout.sets[0]);
    auto deferred_set = crd::make_descriptor_set(context, deferred_pipeline.layout.sets[0]);
    auto gbuffer_set = crd::make_descriptor_set(context, main_pipeline.layout.sets[0]);
    auto light_data_set = crd::make_descriptor_set(context, main_pipeline.layout.sets[1]);

    std::size_t frames = 0;
    double last_frame = 0, fps = 0;
    while (!window.is_closed()) {
        const auto [commands, image, index, wait, signal, done] = crd::acquire_frame(context, renderer, window, swapchain);
        const auto scene = build_scene(draw_cmds, black->info());
        const auto current_frame = crd::current_time();
        const auto delta_time = current_frame - last_frame;
        last_frame = current_frame;
        camera.update(window, delta_time);
        fps += delta_time;
        ++frames;
        dir_lights[0].direction = glm::vec4(
            50.0f * std::cos(crd::current_time() / 6),
            150.0f,
            50.0f * std::sin(crd::current_time() / 6),
            1.0f);
        const auto cascades = calculate_cascades(camera, dir_lights[0].direction);

        crd::wait_fence(context, done);
        crd::resize_buffer(context, model_buffer[index], crd::size_bytes(scene.transforms));
        crd::resize_buffer(context, cascades_buffer[index], crd::size_bytes(cascades));
        crd::resize_buffer(context, directional_light_buffer[index], crd::size_bytes(dir_lights));

        CameraUniform camera_data;
        camera_data.projection = camera.projection;
        camera_data.view = camera.view;
        camera_data.position = glm::vec4(camera.position, 1.0);

        model_buffer[index].write(scene.transforms.data(), 0, crd::size_bytes(scene.transforms));
        cascades_buffer[index].write(cascades.data(), 0, crd::size_bytes(cascades));
        camera_buffer[index].write(&camera_data, 0);
        point_light_buffer[index].write(point_lights.data(), 0, sizeof(glm::mat4));
        directional_light_buffer[index].write(dir_lights.data(), 0, crd::size_bytes(dir_lights));

        shadow_set[index]
            .bind(context, shadow_pipeline.bindings["Models"], model_buffer[index].info())
            .bind(context, shadow_pipeline.bindings["Cascades"], cascades_buffer[index].info())
            .bind(context, shadow_pipeline.bindings["textures"], scene.descriptors);
        gbuffer_set[index]
            .bind(context, main_pipeline.bindings["i_position"], deferred_pass.image(1).info())
            .bind(context, main_pipeline.bindings["i_normal"], deferred_pass.image(2).info())
            .bind(context, main_pipeline.bindings["i_specular"], deferred_pass.image(3).info())
            .bind(context, main_pipeline.bindings["i_albedo"], deferred_pass.image(4).info());
        deferred_set[index]
            .bind(context, deferred_pipeline.bindings["Uniforms"], camera_buffer[index].info())
            .bind(context, deferred_pipeline.bindings["Models"], model_buffer[index].info())
            .bind(context, deferred_pipeline.bindings["textures"], scene.descriptors);
        light_data_set[index]
            .bind(context, main_pipeline.bindings["shadow"], shadow_pass.image(0).sample(context.shadow_sampler))
            .bind(context, main_pipeline.bindings["Camera"], camera_buffer[index].info())
            .bind(context, main_pipeline.bindings["DirectionalLights"], directional_light_buffer[index].info())
            .bind(context, main_pipeline.bindings["PointLights"], point_light_buffer[index].info())
            .bind(context, main_pipeline.bindings["Cascades"], cascades_buffer[index].info());

        commands
            .begin()
            .begin_render_pass(shadow_pass, 0)
            .bind_pipeline(shadow_pipeline)
            .set_viewport(crd::inverted_viewport)
            .set_scissor()
            .bind_descriptor_set(0, shadow_set[index]);
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
        commands
            .end_render_pass()
            .begin_render_pass(deferred_pass, 0)
            .bind_pipeline(deferred_pipeline)
            .set_viewport()
            .set_scissor()
            .bind_descriptor_set(0, deferred_set[index]);
        for (const auto& model : scene.models) {
            auto& raw_model = **model.handle;
            for (const auto& submesh : model.submeshes) {
                auto& raw_submesh = raw_model.submeshes[submesh.index];
                const std::uint32_t indices[] = {
                    model.transform,
                    submesh.textures[0],
                    submesh.textures[1],
                    submesh.textures[2]
                };
                commands
                    .push_constants(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, indices, sizeof indices)
                    .bind_static_mesh(*raw_submesh.mesh)
                    .draw_indexed(raw_submesh.indices, model.instances, 0, 0, 0);
            }
        }
        auto& light_cube = models[0]->submeshes[0];
        const std::uint32_t sizes[] = {
            (std::uint32_t)point_lights.size(),
            (std::uint32_t)dir_lights.size(),
        };
        commands
            .next_subpass()
            .bind_pipeline(main_pipeline)
            .bind_descriptor_set(0, gbuffer_set[index])
            .bind_descriptor_set(1, light_data_set[index])
            .push_constants(VK_SHADER_STAGE_FRAGMENT_BIT, sizes, sizeof sizes)
            .draw(3, 1, 0, 0)
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
            .copy_image(deferred_pass.image(0), image)
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
            spdlog::info("average FPS: {}, dt: {}ms", 1 / (fps / frames), (fps / frames) * 1000);
            frames = 0;
            fps = 0;
        }
        crd::poll_events();
    }
    context.graphics->wait_idle();
    context.compute->wait_idle();
    crd::destroy_descriptor_set(context, light_data_set);
    crd::destroy_descriptor_set(context, gbuffer_set);
    crd::destroy_descriptor_set(context, shadow_set);
    crd::destroy_descriptor_set(context, deferred_set);
    crd::destroy_buffer(context, directional_light_buffer);
    crd::destroy_buffer(context, point_light_buffer);
    crd::destroy_buffer(context, model_buffer);
    crd::destroy_buffer(context, camera_buffer);
    crd::destroy_buffer(context, cascades_buffer);
    for (auto& each : models) {
        crd::destroy_static_model(context, *each);
    }
    crd::destroy_static_texture(context, *black);
    crd::destroy_pipeline(context, shadow_pipeline);
    crd::destroy_pipeline(context, deferred_pipeline);
    crd::destroy_pipeline(context, main_pipeline);
    crd::destroy_render_pass(context, deferred_pass);
    crd::destroy_render_pass(context, shadow_pass);
    crd::destroy_swapchain(context, swapchain);
    crd::destroy_renderer(context, renderer);
    crd::destroy_context(context);
    crd::destroy_window(window);
    return 0;
}