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

static inline crd::ComputePipeline::CreateInfo cull_pipeline_info() noexcept {
    return {
        .compute = "../data/shaders/test_fwdp/light_cull.comp.spv"
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
            .source_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dest_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .source_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dest_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
        }, {
            .source_subpass = 0,
            .dest_subpass = crd::external_subpass,
            .source_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dest_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .source_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dest_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
        } },
        .framebuffers = { {
            { 0 }
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
                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
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
    auto depth_pipeline = crd::make_pipeline(context, renderer, depth_pipeline_info(depth_pass));
    auto cull_pipeline = crd::make_pipeline(context, renderer, cull_pipeline_info());
    auto final_pipeline = crd::make_pipeline(context, renderer, final_pipeline_info(final_pass));
    auto black = crd::request_static_texture(context, "../data/textures/black.png", crd::texture_srgb);
    std::vector<crd::Async<crd::StaticModel>> models;
    models.emplace_back(crd::request_static_model(context, "../data/models/sponza/sponza.gltf"));
    std::vector<Draw> draw_cmds = { {
        .model = &models[0],
        .transforms = { {
            .position = glm::vec3(0.0f, -0.5f, 0.0f),
            .rotation = {},
            .scale = glm::vec3(0.02f)
        } }
    } };
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
    auto depth_set = crd::make_descriptor_set(context, depth_pipeline.layout.sets[0]);
    auto main_set = crd::make_descriptor_set(context, final_pipeline.layout.sets[0]);
    window.set_key_callback([&](crd::Key key, crd::KeyState state) {
        switch (key) {
            case crd::key_r: {
                crd_unlikely_if(state == crd::key_pressed) {
                    depth_pipeline = reload_pipelines(context, renderer, depth_pipeline, depth_pipeline_info(depth_pass));
                    cull_pipeline = reload_pipelines(context, renderer, cull_pipeline, cull_pipeline_info());
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

        crd::wait_fence(context, done);
        crd::resize_buffer(context, model_buffer[index], crd::size_bytes(scene.transforms));

        camera_buffer[index].write(&camera_data, 0, sizeof(CameraUniform));
        model_buffer[index].write(scene.transforms.data(), 0, crd::size_bytes(scene.transforms));

        depth_set[index]
            .bind(context, final_pipeline.bindings["Uniforms"], camera_buffer[index].info())
            .bind(context, final_pipeline.bindings["Models"], model_buffer[index].info());
        main_set[index]
            .bind(context, final_pipeline.bindings["Uniforms"], camera_buffer[index].info())
            .bind(context, final_pipeline.bindings["Models"], model_buffer[index].info())
            .bind(context, final_pipeline.bindings["textures"], scene.descriptors);

        commands
            .begin()
            .begin_render_pass(depth_pass, 0)
            .bind_pipeline(depth_pipeline)
            .bind_descriptor_set(0, depth_set[index])
            .set_viewport(crd::inverted_viewport)
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
            .bind_pipeline(cull_pipeline);
            //.bind_descriptor_set(0, compute_cull_set[index])
            //.dispatch();
        commands
            .begin_render_pass(final_pass, 0)
            .bind_pipeline(final_pipeline)
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
            crd::dtl::log("Scene", crd::dtl::severity_info, crd::dtl::type_performance, "Average FPS: %lf, DT: %lf ", 1 / (fps / frames), fps / frames);
            frames = 0;
            fps = 0;
        }
    }
    return 0;
}