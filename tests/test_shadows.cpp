#include <corundum/core/descriptor_set.hpp>
#include <corundum/core/static_texture.hpp>
#include <corundum/core/static_model.hpp>
#include <corundum/core/static_mesh.hpp>
#include <corundum/core/render_pass.hpp>
#include <corundum/core/utilities.hpp>
#include <corundum/core/swapchain.hpp>
#include <corundum/core/constants.hpp>
#include <corundum/core/renderer.hpp>
#include <corundum/core/pipeline.hpp>
#include <corundum/core/context.hpp>
#include <corundum/core/buffer.hpp>
#include <corundum/core/clear.hpp>
#include <corundum/core/async.hpp>

#include <corundum/detail/logger.hpp>

#include <corundum/wm/window.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>

#include <random>
#include <vector>
#include <array>
#include <span>

struct Camera {
    glm::mat4 perspective;
    glm::vec3 position = { 3.5f, 3.5f,  0.0f };
    glm::vec3 front =    { 0.0f, 0.0f, -1.0f };
    glm::vec3 up =       { 0.0f, 1.0f,  0.0f };
    glm::vec3 right =    { 0.0f, 0.0f,  0.0f };
    glm::vec3 world_up = { 0.0f, 1.0f,  0.0f };
    float yaw = -180.0f;
    float pitch = 0.0f;

    void update(const crd::Window& window, double delta_time) noexcept {
        _process_keyboard(window, delta_time);
        perspective = glm::perspective(glm::radians(60.0f), window.width / (float)window.height, 0.1f, 100.0f);
        perspective[1][1] *= -1;

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
    void _process_keyboard(const crd::Window& window, double delta_time) noexcept {
        constexpr auto camera_speed = 2.5f;
        const auto delta_movement = camera_speed * (float)delta_time;
        if (window.key(crd::key_w) == crd::key_pressed) {
            position.x += std::cos(glm::radians(yaw)) * delta_movement;
            position.z += std::sin(glm::radians(yaw)) * delta_movement;
        }
        if (window.key(crd::key_s) == crd::key_pressed) {
            position.x -= std::cos(glm::radians(yaw)) * delta_movement;
            position.z -= std::sin(glm::radians(yaw)) * delta_movement;
        }
        if (window.key(crd::key_a) == crd::key_pressed) {
            position -= right * delta_movement;
        }
        if (window.key(crd::key_d) == crd::key_pressed) {
            position += right * delta_movement;
        }
        if (window.key(crd::key_space) == crd::key_pressed) {
            position += world_up * delta_movement;
        }
        if (window.key(crd::key_left_shift) == crd::key_pressed) {
            position -= world_up * delta_movement;
        }
        if (window.key(crd::key_left) == crd::key_pressed) {
            yaw -= 150 * delta_time;
        }
        if (window.key(crd::key_right) == crd::key_pressed) {
            yaw += 150 * delta_time;
        }
        if (window.key(crd::key_up) == crd::key_pressed) {
            pitch += 150 * delta_time;
        }
        if (window.key(crd::key_down) == crd::key_pressed) {
            pitch -= 150 * delta_time;
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
        std::uint32_t index;
    };
    std::vector<Submesh> submeshes;
    std::uint32_t index;
};

struct Scene {
    std::vector<VkDescriptorImageInfo> descriptors;
    std::vector<Model> models;
};

struct DirectionalLight {
    glm::vec4 direction;
    glm::vec4 diffuse;
    glm::vec4 specular;
};

struct PointLight {
    glm::vec4 position;
    glm::vec4 falloff;
    glm::vec4 diffuse;
    glm::vec4 specular;
};

static inline float random(float min, float max) {
    static std::random_device device;
    static std::mt19937 engine(device());
    return std::uniform_real_distribution<float>(min, max)(engine);
}

static inline Scene build_scene(std::span<crd::Async<crd::StaticModel>> models, VkDescriptorImageInfo fallback) noexcept {
    Scene scene;
    scene.descriptors = { fallback };
    std::unordered_map<void*, std::uint32_t> texture_cache;
    for (std::uint32_t i = 0; auto& model : models) {
        crd_likely_if(model.is_ready()) {
            const auto submeshes_size = model->submeshes.size();
            auto& handle = scene.models.emplace_back();
            scene.descriptors.reserve(scene.descriptors.size() + submeshes_size * 3);
            texture_cache.reserve(texture_cache.size() + submeshes_size * 3);
            handle.submeshes.reserve(submeshes_size);
            handle.index = i;
            for (std::uint32_t j = 0; auto& submesh : model->submeshes) {
                crd_likely_if(submesh.mesh.is_ready()) {
                    std::array<std::uint32_t, 3> indices = {};
                    const auto emplace_descriptor = [&](crd::Async<crd::StaticTexture>* texture, std::uint32_t which) {
                        crd_likely_if(texture) {
                            auto& cached = texture_cache[texture];
                            crd_unlikely_if(cached == 0 && texture->is_ready()) {
                                scene.descriptors.emplace_back((*texture)->info());
                                cached = scene.descriptors.size() - 1;
                            }
                            indices[which] = cached;
                        }
                    };
                    emplace_descriptor(submesh.diffuse, 0);
                    emplace_descriptor(submesh.normal, 1);
                    emplace_descriptor(submesh.specular, 2);
                    handle.submeshes.push_back({
                        .textures = indices,
                        .index = j
                    });
                }
                j++;
            }
        }
        i++;
    }
    return scene;
}

int main() {
    auto window = crd::make_window(1280, 720, "Hello Triangle");
    auto context = crd::make_context();
    auto renderer = crd::make_renderer(context);
    auto swapchain = crd::make_swapchain(context, window);
    auto main_pass = crd::make_render_pass(context, {
        .attachments = { {
            .image = crd::make_image(context, {
                .width   = window.width,
                .height  = window.height,
                .mips    = 1,
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
            .clear   = crd::make_clear_color({ 0.5f, 0.5f, 0.0f, 1.0f }),
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
            .source_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dest_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .source_access = {},
            .dest_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
        } },
        .framebuffers = { {
            .attachments = { 0 }
        } }
    });
    window.on_resize = [&]() {
        main_pass.resize(context, {
            .size = { swapchain.width, swapchain.height },
            .framebuffer = 0,
            .attachments = { 0 }
        });
    };
    std::size_t frames = 0;
    double last_frame = 0, fps = 0;
    while (!window.is_closed()) {
        const auto [commands, image, index] = renderer.acquire_frame(context, window, swapchain);
        const auto current_frame = crd::time();
        const auto delta_time = current_frame - last_frame;
        last_frame = current_frame;
        fps += delta_time;
        ++frames;
        if (fps >= 1.6) {
            crd::detail::log("Scene", crd::detail::severity_info, crd::detail::type_performance, "Average FPS: %lf ", 1 / (fps / frames));
            frames = 0;
            fps = 0;
        }
        commands
            .begin()
            .begin_render_pass(main_pass, 0)
            .end_render_pass()
            .transition_layout({
                .image = &image,
                .mip = 0,
                .level = 0,
                .source_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dest_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .source_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
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
        renderer.present_frame(context, window, swapchain, commands, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        crd::poll_events();
    }
    context.graphics->wait_idle();
    crd::destroy_render_pass(context, main_pass);
    crd::destroy_swapchain(context, swapchain);
    crd::destroy_renderer(context, renderer);
    crd::destroy_context(context);
    crd::destroy_window(window);
    return 0;
}