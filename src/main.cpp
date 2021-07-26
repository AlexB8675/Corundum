#include <corundum/core/descriptor_set.hpp>
#include <corundum/core/static_texture.hpp>
#include <corundum/core/static_model.hpp>
#include <corundum/core/static_mesh.hpp>
#include <corundum/core/render_pass.hpp>
#include <corundum/core/swapchain.hpp>
#include <corundum/core/constants.hpp>
#include <corundum/core/renderer.hpp>
#include <corundum/core/pipeline.hpp>
#include <corundum/core/context.hpp>
#include <corundum/core/buffer.hpp>
#include <corundum/core/clear.hpp>
#include <corundum/core/async.hpp>

#include <corundum/wm/window.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>

#include <vector>
#include <array>
#include <span>

struct Camera {
    glm::mat4 perspective;
    glm::vec3 position = { 1.2f, 0.4f,  0.0f };
    glm::vec3 front =    { 0.0f, 0.0f, -1.0f };
    glm::vec3 up =       { 0.0f, 1.0f,  0.0f };
    glm::vec3 right =    { 0.0f, 0.0f,  0.0f };
    glm::vec3 world_up = { 0.0f, 1.0f,  0.0f };
    float yaw = -180.0f;
    float pitch = 0.0f;

    void update(const crd::wm::Window& window, double delta_time) noexcept {
        _process_keyboard(window, delta_time);
        perspective = glm::perspective(glm::radians(60.0f), window.width / (float)window.height, 0.1f, 100.0f);

        const auto cos_pitch = std::cos(glm::radians(pitch));
        front = glm::normalize(glm::vec3{
            std::cos(glm::radians(yaw)) * cos_pitch,
            std::sin(glm::radians(pitch)),
            std::sin(glm::radians(yaw)) * cos_pitch
        });
        right = glm::normalize(glm::cross(front, world_up));
        up = glm::normalize(glm::cross(right, front));
    }

    glm::mat4 raw() const noexcept {
        return perspective * glm::lookAt(position, position + front, up);
    }
private:
    void _process_keyboard(const crd::wm::Window& window, double delta_time) noexcept {
        constexpr auto camera_speed = 2.5f;
        const auto delta_movement = camera_speed * (float)delta_time;
        if (window.key(crd::wm::key_w) == crd::wm::key_pressed) {
            position.x += std::cos(glm::radians(yaw)) * delta_movement;
            position.z += std::sin(glm::radians(yaw)) * delta_movement;
        }
        if (window.key(crd::wm::key_s) == crd::wm::key_pressed) {
            position.x -= std::cos(glm::radians(yaw)) * delta_movement;
            position.z -= std::sin(glm::radians(yaw)) * delta_movement;
        }
        if (window.key(crd::wm::key_a) == crd::wm::key_pressed) {
            position -= right * delta_movement;
        }
        if (window.key(crd::wm::key_d) == crd::wm::key_pressed) {
            position += right * delta_movement;
        }
        if (window.key(crd::wm::key_space) == crd::wm::key_pressed) {
            position += world_up * delta_movement;
        }
        if (window.key(crd::wm::key_left_shift) == crd::wm::key_pressed) {
            position -= world_up * delta_movement;
        }
        if (window.key(crd::wm::key_left) == crd::wm::key_pressed) {
            yaw -= 0.150f;
        }
        if (window.key(crd::wm::key_right) == crd::wm::key_pressed) {
            yaw += 0.150f;
        }
        if (window.key(crd::wm::key_up) == crd::wm::key_pressed) {
            pitch += 0.150f;
        }
        if (window.key(crd::wm::key_down) == crd::wm::key_pressed) {
            pitch -= 0.150f;
        }
        if (pitch > 89.9f) {
            pitch = 89.9f;
        }
        if (pitch < -89.9f) {
            pitch = -89.9f;
        }
    }
};


struct Model {
    struct Submesh {
        std::array<std::uint32_t, 3> textures;
    };
    std::vector<Submesh> submeshes;
};

struct Scene {
    std::vector<VkDescriptorImageInfo> descriptors;
    std::vector<Model> models;
};

static inline Scene build_scene(std::span<crd::core::Async<crd::core::StaticModel>> models, VkDescriptorImageInfo black) noexcept {
    Scene scene;
    scene.descriptors = { black };
    std::unordered_map<void*, std::uint32_t> texture_cache;
    for (auto& model : models) {
        crd_likely_if(model.is_ready()) {
            const auto submeshes_size = model->submeshes.size();
            auto& handle = scene.models.emplace_back();
            scene.descriptors.reserve(scene.descriptors.size() + submeshes_size * 3);
            handle.submeshes.reserve(submeshes_size);
            for (auto& submesh : model->submeshes) {
                crd_likely_if(submesh.mesh.is_ready()) {
                    std::array<std::uint32_t, 3> indices = {};
                    const auto emplace_descriptor = [&](const auto texture, std::uint32_t which) {
                        crd_likely_if(texture) {
                            auto& index = texture_cache[texture];
                            crd_likely_if(index != 0) {
                                indices[which] = index;
                            } else crd_likely_if(texture->is_ready()) {
                                scene.descriptors.emplace_back((*texture)->info());
                                indices[which] = index = scene.descriptors.size() - 1;
                            }
                        }
                    };
                    emplace_descriptor(submesh.diffuse, 0);
                    emplace_descriptor(submesh.normal, 1);
                    emplace_descriptor(submesh.specular, 2);
                    handle.submeshes.emplace_back().textures = indices;
                }
            }
        }
    }
    return scene;
}

int main() {
    auto window      = crd::wm::make_window(1280, 720, "Sorting Algos");
    auto context     = crd::core::make_context();
    auto renderer    = crd::core::make_renderer(context);
    auto swapchain   = crd::core::make_swapchain(context, window);
    auto render_pass = crd::core::make_render_pass(context, {
        .attachments = { {
            .image = crd::core::make_image(context, {
                .width   = 1280,
                .height  = 720,
                .mips    = 1,
                .format  = swapchain.format,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .usage   = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT
            }),
            .layout = {
                .initial = VK_IMAGE_LAYOUT_UNDEFINED,
                .final   = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
            },
            .clear   = crd::core::make_clear_color({
                24  / 255.0f,
                154 / 255.0f,
                207 / 255.0f,
                1.0f
            }),
            .owning  = true,
            .discard = false
        }, {
            .image = crd::core::make_image(context, {
                .width   = 1280,
                .height  = 720,
                .mips    = 1,
                .format  = VK_FORMAT_D32_SFLOAT_S8_UINT,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .usage   = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
            }),
            .layout = {
                .initial = VK_IMAGE_LAYOUT_UNDEFINED,
                .final   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            },
            .clear   = crd::core::make_clear_depth({ 1.0, 0 }),
            .owning  = true,
            .discard = true
        } },
        .subpasses = { {
            .attachments = { 0, 1 },
            .preserve    = {},
            .input       = {}
        } },
        .dependencies = { {
            .source_subpass = crd::core::external_subpass,
            .dest_subpass   = 0,
            .source_stage   = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dest_stage     = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .source_access  = {},
            .dest_access    = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
        } },
        .framebuffers = {
            { { 0, 1 } }
        }
    });
    auto pipeline = crd::core::make_pipeline(context, renderer, {
        .vertex = "data/shaders/main.vert.spv",
        .fragment = "data/shaders/main.frag.spv",
        .render_pass = &render_pass,
        .attributes = {
            crd::core::vertex_attribute_vec3,
            crd::core::vertex_attribute_vec3,
            crd::core::vertex_attribute_vec2,
            crd::core::vertex_attribute_vec3,
            crd::core::vertex_attribute_vec3
        },
        .states = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        },
        .subpass = 0,
        .depth = true
    });
    auto black = crd::core::request_static_texture(context, "data/textures/black.png", crd::core::texture_srgb);
    std::vector<crd::core::Async<crd::core::StaticModel>> models;
    models.emplace_back(crd::core::request_static_model(context, "data/models/sponza/sponza.obj"));
    Camera camera;
    std::vector transforms{
        glm::scale(glm::mat4(1.0f), glm::vec3(0.02f))
    };
    auto camera_buffer = crd::core::make_buffer<>(context, sizeof(glm::mat4), crd::core::uniform_buffer);
    auto model_buffer = crd::core::make_buffer<>(context, sizeof(glm::mat4), crd::core::storage_buffer);
    auto set = crd::core::make_descriptor_set<>(context, pipeline.descriptors[0]);
    double delta_time = 0, last_frame = 0;
    while (!crd::wm::is_closed(window)) {
        const auto [commands, image, index] = renderer.acquire_frame(context, swapchain);
        const auto scene = build_scene(models, black->info());
        const auto current_frame = crd::wm::time();
        delta_time = current_frame - last_frame;
        last_frame = current_frame;
        model_buffer[index].resize(context, std::span(transforms).size_bytes());
        camera_buffer[index].write(glm::value_ptr(camera.raw()), 0);
        model_buffer[index].write(transforms.data(), 0);
        set[index].bind(context, pipeline.bindings["Camera"], camera_buffer[index].info());
        set[index].bind(context, pipeline.bindings["Models"], model_buffer[index].info());
        set[index].bind(context, pipeline.bindings["textures"], scene.descriptors);

        commands
            .begin()
            .begin_render_pass(render_pass, 0)
            .set_viewport(0)
            .set_scissor(0)
            .bind_pipeline(pipeline)
            .bind_descriptor_set(set[index]);
        for (std::uint32_t i = 0; const auto& model : scene.models) {
            auto& raw_model = *models[i++];
            for (std::uint32_t j = 0; const auto& submesh : model.submeshes) {
                auto& raw_submesh = raw_model.submeshes[j++];
                std::array indices{
                    0u,
                    submesh.textures[0],
                    submesh.textures[1],
                    submesh.textures[2]
                };
                commands
                    .push_constants(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                    indices.data(),
                                    std::span(indices).size_bytes())
                    .bind_static_mesh(*raw_submesh.mesh)
                    .draw_indexed(raw_submesh.indices, 1, 0, 0, 0);
            }
        }
        commands
            .end_render_pass()
            .insert_layout_transition({
                .image = &image,
                .mip = 0,
                .level = 0,
                .source_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dest_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .source_access = {},
                .dest_access = VK_ACCESS_TRANSFER_WRITE_BIT,
                .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                .new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            })
            .copy_image(render_pass.image(0), image)
            .insert_layout_transition({
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
        renderer.present_frame(context, swapchain, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        crd::wm::poll_events();
        camera.update(window, delta_time);
    }
    context.graphics->wait_idle();
    crd::core::destroy_descriptor_set(context, set);
    crd::core::destroy_buffer(context, model_buffer);
    crd::core::destroy_buffer(context, camera_buffer);
    for (auto& each : models) {
        crd::core::destroy_static_model(context, *each);
    }
    crd::core::destroy_static_texture(context, *black);
    crd::core::destroy_pipeline(context, pipeline);
    crd::core::destroy_render_pass(context, render_pass);
    crd::core::destroy_swapchain(context, swapchain);
    crd::core::destroy_renderer(context, renderer);
    crd::core::destroy_context(context);
    crd::wm::destroy_window(window);
    return 0;
}
