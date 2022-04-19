#include <common.hpp>

struct CameraUniform {
    glm::mat4 projection;
    glm::mat4 view;
    glm::vec3 position;
    float frustum_size;
};

static inline crd::GraphicsPipeline::CreateInfo shadow_pipeline_info(const crd::RenderPass& pass) noexcept {
    return {
        .vertex = "../data/shaders/playground/shadow.vert.spv",
        .geometry = "../data/shaders/playground/shadow.geom.spv",
        .fragment = "../data/shaders/playground/shadow.frag.spv",
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

static inline crd::GraphicsPipeline::CreateInfo final_pipeline_info(const crd::RenderPass& pass) noexcept {
    return {
        .vertex = "../data/shaders/playground/final.vert.spv",
        .geometry = nullptr,
        .fragment = "../data/shaders/playground/final.frag.spv",
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

int main() {
    auto window = crd::make_window(1280, 720, "Playground");
    auto context = crd::make_context();
    auto renderer = crd::make_renderer(context);
    auto swapchain = crd::make_swapchain(context, window);
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
            .image = crd::make_image(context, {
                .width = window.width,
                .height = window.height,
                .mips = 1,
                .layers = 1,
                .format = VK_FORMAT_D32_SFLOAT,
                .aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
            }),
            .layout = {
                .initial = VK_IMAGE_LAYOUT_UNDEFINED,
                .final = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            },
            .clear = crd::make_clear_depth({ 1.0f, 0 }),
            .owning = true,
            .discard = true
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
                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dest_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                          VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .source_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dest_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                           VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                           VK_ACCESS_SHADER_READ_BIT
        }, {
            .source_subpass = 0,
            .dest_subpass = crd::external_subpass,
            .source_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dest_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .source_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dest_access = VK_ACCESS_TRANSFER_READ_BIT
        } },
        .framebuffers = { {
            { 0, 1 }
        } }
    });
    auto shadow_pipeline = crd::make_pipeline(context, renderer, shadow_pipeline_info(shadow_pass));
    auto final_pipeline = crd::make_pipeline(context, renderer, final_pipeline_info(final_pass));
    auto black = crd::request_static_texture(context, "../data/textures/black.png", crd::texture_srgb);
    std::vector<crd::Async<crd::StaticModel>> models;
    models.emplace_back(crd::request_static_model(context, "../data/models/cube/cube.obj"));
    models.emplace_back(crd::request_static_model(context, "../data/models/sponza/sponza.gltf"));
    models.emplace_back(crd::request_static_model(context, "../data/models/dragon/dragon.obj"));
    models.emplace_back(crd::request_static_model(context, "../data/models/suzanne/suzanne.obj"));
    models.emplace_back(crd::request_static_model(context, "../data/models/plane/plane.obj"));
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
        .model = &models[4],
        .transforms = { {
            .position = glm::vec3(0.0f),
            .rotation = {},
            .scale = glm::vec3(1.0f)
        } }
    } });
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
    auto directional_lights_buffer = crd::make_buffer(context, {
        .type = crd::uniform_buffer,
        .usage = crd::host_visible,
        .capacity = sizeof(DirectionalLight[max_directional_lights])
    });
    auto shadow_set = crd::make_descriptor_set(context, shadow_pipeline.layout.sets[0]);
    auto main_set = crd::make_descriptor_set(context, final_pipeline.layout.sets[0]);
    auto light_data_set = crd::make_descriptor_set(context, final_pipeline.layout.sets[1]);
    window.set_resize_callback([&]() {
        final_pass.resize(context, {
            .size = { swapchain.width, swapchain.height },
            .framebuffer = 0,
            .attachments = { 0, 1 },
            .references = {}
        });
    });
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
                    final_pipeline = reload_pipelines(context, renderer, final_pipeline, final_pipeline_info(final_pass));
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
        camera_data.frustum_size = 2 * Camera::near * std::tan(Camera::fov * 0.5) * camera.aspect;

        crd::wait_fence(context, done);

        sun_dlight.direction = glm::vec4(
            50.0f * std::cos(crd::current_time() / 6),
            150.0f,
            50.0f * std::sin(crd::current_time() / 6),
            1.0f);
        const auto cascades = calculate_cascades(camera, sun_dlight.direction);
        crd::resize_buffer(context, model_buffer[index], crd::size_bytes(scene.transforms));

        camera_buffer[index].write(&camera_data, 0, sizeof(CameraUniform));
        cascades_buffer[index].write(cascades.data(), 0);
        directional_lights_buffer[index].write(&sun_dlight, 0, sizeof sun_dlight);
        model_buffer[index].write(scene.transforms.data(), 0, crd::size_bytes(scene.transforms));

        shadow_set[index]
            .bind(context, shadow_pipeline.bindings["Models"], model_buffer[index].info())
            .bind(context, shadow_pipeline.bindings["Cascades"], cascades_buffer[index].info())
            .bind(context, shadow_pipeline.bindings["textures"], scene.descriptors);
        main_set[index]
            .bind(context, final_pipeline.bindings["Uniforms"], camera_buffer[index].info())
            .bind(context, final_pipeline.bindings["Models"], model_buffer[index].info())
            .bind(context, final_pipeline.bindings["textures"], scene.descriptors);
        light_data_set[index]
            .bind(context, final_pipeline.bindings["DirectionalLights"], directional_lights_buffer[index].info())
            .bind(context, final_pipeline.bindings["Cascades"], cascades_buffer[index].info())
            .bind(context, final_pipeline.bindings["shadow"], shadow_pass.image(0).sample(context.shadow_sampler));

        commands
            .begin()
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
        commands
            .end_render_pass()
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
                    1
                };
                commands
                    .push_constants(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, indices, sizeof indices)
                    .bind_static_mesh(*raw_submesh.mesh)
                    .draw_indexed(raw_submesh.indices, model.instances, 0, 0, 0);
            }
        }
        commands
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
            spdlog::info("average FPS: {}, dt: {}ms", 1 / (fps / frames), (fps / frames) * 1000);
            frames = 0;
            fps = 0;
        }
    }
    return 0;
}
