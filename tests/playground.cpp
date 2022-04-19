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
    window.set_resize_callback([&]() {

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
        commands
            .begin()
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
            //.copy_image(final_pass.image(0), image)
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
