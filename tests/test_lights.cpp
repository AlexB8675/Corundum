#include <common.hpp>

int main() {
    auto window = crd::make_window(1280, 720, "Test Deferred Lights");
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
            .clear   = crd::make_clear_color({}),
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
            .source_stage   = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dest_stage     = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .source_access  = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dest_access    = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
        }, {
            .source_subpass = 0,
            .dest_subpass   = 1,
            .source_stage   = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dest_stage     = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .source_access  = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
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
    auto main_pipeline = crd::make_pipeline(context, renderer, {
        .vertex = "../data/shaders/main.vert.spv",
        .geometry = nullptr,
        .fragment = "../data/shaders/main.frag.spv",
        .render_pass = &deferred_pass,
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
        .render_pass = &deferred_pass,
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
        .subpass = 1,
        .depth = true
    });
    auto combine_pipeline = crd::make_pipeline(context, renderer, {
        .vertex = "../data/shaders/combine.vert.spv",
        .geometry = nullptr,
        .fragment = "../data/shaders/combine.frag.spv",
        .render_pass = &deferred_pass,
        .attributes = {},
        .attachments = {
            crd::color_attachment_auto
        },
        .states = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        },
        .cull = VK_CULL_MODE_NONE,
        .subpass = 1,
        .depth = false
    });
    window.on_resize = [&]() {
        deferred_pass.resize(context, {
            .size = { swapchain.width, swapchain.height },
            .framebuffer = 0,
            .attachments = { 0, 1, 2, 3, 4, 5 }
        });
    };
    const auto nlights = 128;
    std::vector<PointLight> lights = {};
    lights.reserve(nlights);
    for (int i = 0; i < nlights; ++i) {
        lights.push_back({
            .position = glm::vec4(random(-24, 24), -0.2f, random(-24, 24), 0.0f),
            .falloff = glm::vec4(1.0f, 0.20f, 0.18f, 0.0f),
            .diffuse = glm::vec4(random(0, 1), random(0, 1), random(0, 1), 0.0f),
            .specular = glm::vec4(1.0f)
        });
    }
    std::vector<glm::vec4> light_colors;
    light_colors.reserve(lights.size());
    for (const auto& light : lights) {
        light_colors.emplace_back(light.diffuse);
    }
    auto black = crd::request_static_texture(context, "../data/textures/black.png", crd::texture_srgb);
    std::vector<crd::Async<crd::StaticModel>> models;
    models.emplace_back(crd::request_static_model(context, "../data/models/cube/cube.obj"));
    models.emplace_back(crd::request_static_model(context, "../data/models/plane/plane.obj"));
    std::vector<Draw> draw_cmds = { {
        .model = &models[0],
        .transforms = { {
            .position = glm::vec3(0.0f),
            .rotation = {},
            .scale = glm::vec3(1.0f)
        } }
    }, {
        .model = &models[1],
        .transforms = { {
            .position = glm::vec3(0.0f, -0.5f, 0.0f),
            .rotation = {},
            .scale = glm::vec3(1.0f)
        } }
    } };
    Camera camera;
    std::vector<glm::mat4> light_ts;
    light_ts.reserve(lights.size());
    for (const auto& light : lights) {
        light_ts.emplace_back(glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(light.position)), glm::vec3(0.1f)));
    }
    auto camera_buffer = crd::make_buffer(context, sizeof(glm::mat4) * 2, crd::uniform_buffer);
    auto model_buffer = crd::make_buffer(context, sizeof(glm::mat4), crd::storage_buffer);
    auto light_model_buffer = crd::make_buffer(context, crd::size_bytes(light_ts), crd::storage_buffer);
    auto light_color_buffer = crd::make_buffer(context, crd::size_bytes(lights), crd::storage_buffer);
    auto light_uniform_buffer = crd::make_buffer(context, sizeof(glm::vec4), crd::uniform_buffer);
    auto point_light_buffer = crd::make_buffer(context, crd::size_bytes(lights), crd::storage_buffer);
    auto directional_light_buffer = crd::make_buffer(context, sizeof(glm::mat4), crd::storage_buffer);
    auto main_set = crd::make_descriptor_set(context, main_pipeline.layout.sets[0]);
    auto light_set = crd::make_descriptor_set(context, light_pipeline.layout.sets[0]);
    auto gbuffer_set = crd::make_descriptor_set<1>(context, combine_pipeline.layout.sets[0]);
    auto light_data_set = crd::make_descriptor_set(context, combine_pipeline.layout.sets[1]);
    std::size_t frames = 0;
    double delta_time = 0, last_frame = 0, fps = 0;
    while (!window.is_closed()) {
        const auto [commands, image, index] = renderer.acquire_frame(context, window, swapchain);
        const auto scene = build_scene(draw_cmds, black->info());
        const auto current_frame = crd::time();
        camera.update(window, delta_time);
        ++frames;
        delta_time = current_frame - last_frame;
        last_frame = current_frame;
        fps += delta_time;
        if (fps >= 1.6) {
            crd::detail::log("Scene", crd::detail::severity_info, crd::detail::type_performance, "Average FPS: %lf ", 1 / (fps / frames));
            frames = 0;
            fps = 0;
        }
        crd::resize_buffer(context, model_buffer[index], crd::size_bytes(scene.transforms));
        crd::resize_buffer(context, point_light_buffer[index], crd::size_bytes(lights));
        crd::resize_buffer(context, light_color_buffer[index], crd::size_bytes(light_colors));

        model_buffer[index].write(scene.transforms.data(), 0);
        light_model_buffer[index].write(light_ts.data(), 0);
        camera_buffer[index].write(glm::value_ptr(camera.projection), 0, sizeof(glm::mat4));
        camera_buffer[index].write(glm::value_ptr(camera.view), sizeof(glm::mat4), sizeof(glm::mat4));
        point_light_buffer[index].write(lights.data(), 0, crd::size_bytes(lights));
        light_color_buffer[index].write(light_colors.data(), 0, crd::size_bytes(light_colors));
        light_uniform_buffer[index].write(&camera.position, 0, sizeof camera.position);
        main_set[index]
            .bind(context, main_pipeline.bindings["Uniforms"], camera_buffer[index].info())
            .bind(context, main_pipeline.bindings["Models"], model_buffer[index].info())
            .bind(context, main_pipeline.bindings["textures"], scene.descriptors);
        light_set[index]
            .bind(context, light_pipeline.bindings["Uniforms"], camera_buffer[index].info())
            .bind(context, light_pipeline.bindings["Models"], light_model_buffer[index].info())
            .bind(context, light_pipeline.bindings["Colors"], light_color_buffer[index].info());
        light_data_set[index]
            .bind(context, combine_pipeline.bindings["Uniforms"], light_uniform_buffer[index].info())
            // .bind(context, combine_pipeline.bindings["DirectionalLights"], directional_light_buffer[index].info())
            .bind(context, combine_pipeline.bindings["PointLights"], point_light_buffer[index].info());
        gbuffer_set
            .bind(context, combine_pipeline.bindings["i_position"], deferred_pass.image(1).info())
            .bind(context, combine_pipeline.bindings["i_normal"], deferred_pass.image(2).info())
            .bind(context, combine_pipeline.bindings["i_specular"], deferred_pass.image(3).info())
            .bind(context, combine_pipeline.bindings["i_albedo"], deferred_pass.image(4).info());
        commands
            .begin()
            .begin_render_pass(deferred_pass, 0)
            .set_viewport(crd::inverted_viewport)
            .set_scissor()
            .bind_pipeline(main_pipeline)
            .bind_descriptor_set(0, main_set[index]);
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
            .next_subpass()
            .bind_pipeline(combine_pipeline)
            .bind_descriptor_set(0, gbuffer_set)
            .bind_descriptor_set(1, light_data_set[index])
            .draw(3, 1, 0, 0)
            .bind_pipeline(light_pipeline)
            .bind_descriptor_set(0, light_set[index])
            .bind_static_mesh(*light_cube.mesh)
            .draw_indexed(light_cube.indices, nlights, 0, 0, 0)
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
        renderer.present_frame(context, {
            commands,
            window,
            swapchain,
            {},
            VK_PIPELINE_STAGE_TRANSFER_BIT
        });
        crd::poll_events();
    }
    context.graphics->wait_idle();
    crd::destroy_descriptor_set(context, gbuffer_set);
    crd::destroy_descriptor_set(context, light_data_set);
    crd::destroy_descriptor_set(context, light_set);
    crd::destroy_descriptor_set(context, main_set);
    crd::destroy_buffer(context, light_uniform_buffer);
    crd::destroy_buffer(context, light_color_buffer);
    crd::destroy_buffer(context, light_model_buffer);
    crd::destroy_buffer(context, directional_light_buffer);
    crd::destroy_buffer(context, point_light_buffer);
    crd::destroy_buffer(context, model_buffer);
    crd::destroy_buffer(context, camera_buffer);
    for (auto& each : models) {
        crd::destroy_static_model(context, *each);
    }
    crd::destroy_static_texture(context, *black);
    crd::destroy_pipeline(context, combine_pipeline);
    crd::destroy_pipeline(context, light_pipeline);
    crd::destroy_pipeline(context, main_pipeline);
    crd::destroy_render_pass(context, deferred_pass);
    crd::destroy_swapchain(context, swapchain);
    crd::destroy_renderer(context, renderer);
    crd::destroy_context(context);
    crd::destroy_window(window);
    return 0;
}
