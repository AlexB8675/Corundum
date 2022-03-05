#include <common.hpp>

int main() {
    auto window = crd::make_window(1280, 720, "Test Simple Shadow");
    auto context = crd::make_context();
    auto renderer = crd::make_renderer(context);
    auto swapchain = crd::make_swapchain(context, window);
    auto shadow_pass = crd::make_render_pass(context, {
        .attachments = { {
            .image = crd::make_image(context, {
                .width   = 4096,
                .height  = 4096,
                .mips    = 1,
                .layers  = 1,
                .format  = VK_FORMAT_D32_SFLOAT,
                .aspect  = VK_IMAGE_ASPECT_DEPTH_BIT,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .usage   = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                           VK_IMAGE_USAGE_SAMPLED_BIT,
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
            .source_access = {},
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
    auto main_pass = crd::make_render_pass(context, {
        .attachments = { {
            .image = crd::make_image(context, {
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
            .clear   = crd::make_clear_color({ 0.0f, 0.0f, 0.0f, 1.0f }),
            .owning  = true,
            .discard = false
        }, {
            .image = crd::make_image(context, {
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
            .source_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .dest_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .source_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dest_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
        }, {
            .source_subpass = 0,
            .dest_subpass = crd::external_subpass,
            .source_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dest_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .source_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dest_access = VK_ACCESS_TRANSFER_READ_BIT
        } },
        .framebuffers = { {
            .attachments = { 0, 1 }
        } }
    });
    auto shadow_pipeline = crd::make_pipeline(context, renderer, {
        .vertex = "../data/shaders/test_shadows/shadow.vert.spv",
        .geometry = nullptr,
        .fragment = nullptr,
        .render_pass = &shadow_pass,
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
            VK_DYNAMIC_STATE_SCISSOR,
        },
        .cull = VK_CULL_MODE_FRONT_BIT,
        .subpass = 0,
        .depth = true
    });
    auto main_pipeline = crd::make_pipeline(context, renderer, {
        .vertex = "../data/shaders/test_shadows/main.vert.spv",
        .geometry = nullptr,
        .fragment = "../data/shaders/test_shadows/main.frag.spv",
        .render_pass = &main_pass,
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
            VK_DYNAMIC_STATE_SCISSOR,
        },
        .cull = VK_CULL_MODE_BACK_BIT,
        .subpass = 0,
        .depth = true
    });
    auto light_pipeline = crd::make_pipeline(context, renderer, {
        .vertex = "../data/shaders/light.vert.spv",
        .geometry = nullptr,
        .fragment = "../data/shaders/light.frag.spv",
        .render_pass = &main_pass,
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
            VK_DYNAMIC_STATE_SCISSOR,
        },
        .cull = VK_CULL_MODE_BACK_BIT,
        .subpass = 0,
        .depth = true
    });
    window.on_resize = [&]() {
        main_pass.resize(context, {
            .size = { swapchain.width, swapchain.height },
            .framebuffer = 0,
            .attachments = { 0, 1 }
        });
    };
    auto black = crd::request_static_texture(context, "../data/textures/black.png", crd::texture_srgb);
    auto models = std::vector<crd::Async<crd::StaticModel>>();
    models.emplace_back(crd::request_static_model(context, "../data/models/cube/cube.obj"));
    models.emplace_back(crd::request_static_model(context, "../data/models/plane/plane.obj"));
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
                .angle = glm::radians(60.0f)
            },
            .scale = glm::vec3(0.25f)
        } }
    }, {
        .model = &models[1],
        .transforms = { {
            .position = glm::vec3(0.0f),
            .rotation = {},
            .scale = glm::vec3(1.0f)
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
    std::vector<PointLight> lights = { {
        .position = glm::vec4(0.0f),
        .falloff = glm::vec4(1.0f, 0.078f, 0.096f, 0.0f),
        .diffuse = glm::vec4(1.0f),
        .specular = glm::vec4(1.0f)
    } };
    std::vector<glm::vec4> light_colors;
    light_colors.reserve(lights.size());
    for (const auto& light : lights) {
        light_colors.emplace_back(light.diffuse);
    }
    Camera camera;
    std::vector<glm::mat4> light_ts;
    light_ts.reserve(lights.size());
    for (const auto& light : lights) {
        light_ts.emplace_back(glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(light.position)), glm::vec3(0.1f)));
    }
    auto shadow_camera_buffer = crd::make_buffer(context, sizeof(glm::mat4), crd::uniform_buffer);
    auto camera_buffer = crd::make_buffer(context, sizeof(glm::mat4) * 3, crd::uniform_buffer);
    auto model_buffer = crd::make_buffer(context, sizeof(glm::mat4), crd::storage_buffer);
    auto light_model_buffer = crd::make_buffer(context, crd::size_bytes(light_ts), crd::storage_buffer);
    auto light_color_buffer = crd::make_buffer(context, crd::size_bytes(lights), crd::storage_buffer);
    auto light_uniform_buffer = crd::make_buffer(context, sizeof(glm::vec4), crd::uniform_buffer);
    auto point_light_buffer = crd::make_buffer(context, crd::size_bytes(lights), crd::storage_buffer);
    auto directional_light_buffer = crd::make_buffer(context, sizeof(glm::mat4), crd::storage_buffer);
    auto shadow_set = crd::make_descriptor_set(context, shadow_pipeline.layout.sets[0]);
    auto main_set = crd::make_descriptor_set(context, main_pipeline.layout.sets[0]);
    auto light_set = crd::make_descriptor_set(context, light_pipeline.layout.sets[0]);
    auto light_data_set = crd::make_descriptor_set(context, main_pipeline.layout.sets[1]);
    std::size_t frames = 0;
    double last_frame = 0, fps = 0;
    while (!window.is_closed()) {
        const auto [commands, image, index] = renderer.acquire_frame(context, window, swapchain);
        const auto scene = build_scene(draw_cmds, black->info());
        const auto current_frame = crd::current_time();
        const auto delta_time = current_frame - last_frame;
        last_frame = current_frame;
        fps += delta_time;
        ++frames;
        if (fps >= 1.6) {
            crd::detail::log("Scene", crd::detail::severity_info, crd::detail::type_performance, "Average FPS: %lf ", 1 / (fps / frames));
            frames = 0;
            fps = 0;
        }
        lights[0].position = glm::vec4(
            8.0f * std::sin(crd::current_time() / 4),
            6.0f,
            8.0f * std::cos(crd::current_time() / 4),
            0.0f);
        for (std::size_t i = 0; const auto& light : lights) {
            light_ts[i++] = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(light.position)), glm::vec3(0.1f));
        }
        glm::mat4 shadow_camera =
            glm::ortho(-20.0f, 20.0f, -20.0f, 20.0f, 1.0f, 20.0f) *
            glm::lookAt(glm::vec3(lights[0].position),
                        glm::vec3(0.0f, 0.0f, 0.0f),
                        glm::vec3(0.0f, 1.0f, 0.0f));

        crd::resize_buffer(context, model_buffer[index], crd::size_bytes(scene.transforms));
        crd::resize_buffer(context, point_light_buffer[index], crd::size_bytes(lights));
        crd::resize_buffer(context, light_color_buffer[index], crd::size_bytes(light_colors));

        model_buffer[index].write(scene.transforms.data(), 0, crd::size_bytes(scene.transforms));
        light_model_buffer[index].write(light_ts.data(), 0, crd::size_bytes(light_ts));
        shadow_camera_buffer[index].write(glm::value_ptr(shadow_camera), 0, sizeof shadow_camera);
        camera_buffer[index].write(glm::value_ptr(camera.projection), 0, sizeof(glm::mat4));
        camera_buffer[index].write(glm::value_ptr(camera.view), sizeof(glm::mat4), sizeof(glm::mat4));
        camera_buffer[index].write(glm::value_ptr(shadow_camera), sizeof(glm::mat4) * 2, sizeof(glm::mat4));
        point_light_buffer[index].write(lights.data(), 0, crd::size_bytes(lights));
        light_color_buffer[index].write(light_colors.data(), 0, crd::size_bytes(light_colors));
        light_uniform_buffer[index].write(&camera.position, 0, sizeof camera.position);

        shadow_set[index]
            .bind(context, shadow_pipeline.bindings["Uniforms"], shadow_camera_buffer[index].info())
            .bind(context, shadow_pipeline.bindings["Models"], model_buffer[index].info());
        main_set[index]
            .bind(context, main_pipeline.bindings["Uniforms"], camera_buffer[index].info())
            .bind(context, main_pipeline.bindings["Models"], model_buffer[index].info())
            .bind(context, main_pipeline.bindings["textures"], scene.descriptors);
        light_set[index]
            .bind(context, light_pipeline.bindings["Uniforms"], camera_buffer[index].info())
            .bind(context, light_pipeline.bindings["Models"], light_model_buffer[index].info())
            .bind(context, light_pipeline.bindings["Colors"], light_color_buffer[index].info());
        light_data_set[index]
            .bind(context, main_pipeline.bindings["ViewPos"], light_uniform_buffer[index].info())
                // .bind(context, main_pipeline.bindings["DirectionalLights"], directional_light_buffer[index].info())
            .bind(context, main_pipeline.bindings["PointLights"], point_light_buffer[index].info())
            .bind(context, main_pipeline.bindings["shadow"], shadow_pass.image(0).sample(context.shadow_sampler));
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
                    model.transform
                };
                commands
                    .push_constants(VK_SHADER_STAGE_VERTEX_BIT, indices, sizeof(indices))
                    .bind_static_mesh(*raw_submesh.mesh)
                    .draw_indexed(raw_submesh.indices, model.instances, 0, 0, 0);
            }
        }
        commands
            .end_render_pass()
            .begin_render_pass(main_pass, 0)
            .bind_pipeline(main_pipeline)
            .set_viewport(crd::inverted_viewport)
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
                    submesh.textures[2]
                };
                commands
                    .push_constants(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, indices, sizeof(indices))
                    .bind_static_mesh(*raw_submesh.mesh)
                    .draw_indexed(raw_submesh.indices, model.instances, 0, 0, 0);
            }
        }
        auto& light_cube = models[0]->submeshes[0];
        commands
            .bind_pipeline(light_pipeline)
            .bind_descriptor_set(0, light_set[index])
            .bind_static_mesh(*light_cube.mesh)
            .draw_indexed(light_cube.indices, lights.size(), 0, 0, 0)
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
            .copy_image(main_pass.image(0), image)
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
        renderer.present_frame(context, {
            commands,
            window,
            swapchain,
            {},
            VK_PIPELINE_STAGE_TRANSFER_BIT
        });
        camera.update(window, delta_time);
        crd::poll_events();
    }
    context.graphics->wait_idle();
    crd::destroy_descriptor_set(context, light_data_set);
    crd::destroy_descriptor_set(context, light_set);
    crd::destroy_descriptor_set(context, shadow_set);
    crd::destroy_descriptor_set(context, main_set);
    crd::destroy_buffer(context, light_uniform_buffer);
    crd::destroy_buffer(context, light_color_buffer);
    crd::destroy_buffer(context, directional_light_buffer);
    crd::destroy_buffer(context, point_light_buffer);
    crd::destroy_buffer(context, light_model_buffer);
    crd::destroy_buffer(context, model_buffer);
    crd::destroy_buffer(context, camera_buffer);
    crd::destroy_buffer(context, shadow_camera_buffer);
    for (auto& each : models) {
        crd::destroy_static_model(context, *each);
    }
    crd::destroy_static_texture(context, *black);
    crd::destroy_pipeline(context, shadow_pipeline);
    crd::destroy_pipeline(context, light_pipeline);
    crd::destroy_pipeline(context, main_pipeline);
    crd::destroy_render_pass(context, main_pass);
    crd::destroy_render_pass(context, shadow_pass);
    crd::destroy_swapchain(context, swapchain);
    crd::destroy_renderer(context, renderer);
    crd::destroy_context(context);
    crd::destroy_window(window);
    return 0;
}