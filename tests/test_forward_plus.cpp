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
    glm::vec3 position = { 1.2f, 0.4f, -3.0f };
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

static inline Scene build_scene(std::span<crd::Async<crd::StaticModel>*> models, VkDescriptorImageInfo fallback) noexcept {
    Scene scene;
    scene.descriptors = { fallback };
    std::unordered_map<void*, std::uint32_t> texture_cache;
    for (std::uint32_t i = 0; auto& model : models) {
        crd_likely_if(model->is_ready()) {
            const auto submeshes_size = (*model)->submeshes.size();
            auto& handle = scene.models.emplace_back();
            scene.descriptors.reserve(scene.descriptors.size() + submeshes_size * 3);
            texture_cache.reserve(texture_cache.size() + submeshes_size * 3);
            handle.submeshes.reserve(submeshes_size);
            handle.index = i;
            for (std::uint32_t j = 0; auto& submesh : (*model)->submeshes) {
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

static inline float random(float min, float max) {
    static std::random_device device;
    static std::mt19937 engine(device());
    return std::uniform_real_distribution<float>(min, max)(engine);
}

int main() {
    auto window = crd::make_window(1280, 720, "Test");
    auto context = crd::make_context();
    auto renderer = crd::make_renderer(context);
    auto swapchain = crd::make_swapchain(context, window);
    auto deferred_pass = crd::make_render_pass(context, {
        .attachments = { {
            .image = crd::make_image(context, { // Final Color.
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
            .clear   = crd::make_clear_color({}),
            .owning  = true,
            .discard = false
        }, {
            .image = crd::make_image(context, { // Position.
                .width   = window.width,
                .height  = window.height,
                .mips    = 1,
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
            .source_stage   = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dest_stage     = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .source_access  = {},
            .dest_access    = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
        }, {
            .source_subpass = 0,
            .dest_subpass   = 1,
            .source_stage   = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dest_stage     = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .source_access  = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
            .dest_access    = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
        } },
        .framebuffers = {
            { { 0, 1, 2, 3, 4, 5 } }
        }
    });
    auto main_pipeline = crd::make_graphics_pipeline(context, renderer, {
        .vertex = "data/shaders/main.vert.spv",
        .fragment = "data/shaders/main.frag.spv",
        .render_pass = &deferred_pass,
        .attributes = {
            crd::vertex_attribute_vec3,
            crd::vertex_attribute_vec3,
            crd::vertex_attribute_vec2,
            crd::vertex_attribute_vec3,
            crd::vertex_attribute_vec3
        },
        .states = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        },
        .cull = VK_CULL_MODE_BACK_BIT,
        .subpass = 0,
        .depth = true
    });
    auto light_pipeline = crd::make_graphics_pipeline(context, renderer, {
        .vertex = "data/shaders/light.vert.spv",
        .fragment = "data/shaders/light.frag.spv",
        .render_pass = &deferred_pass,
        .attributes = {
            crd::vertex_attribute_vec3,
            crd::vertex_attribute_vec3,
            crd::vertex_attribute_vec2,
            crd::vertex_attribute_vec3,
            crd::vertex_attribute_vec3
        },
        .states = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        },
        .cull = VK_CULL_MODE_BACK_BIT,
        .subpass = 1,
        .depth = true
    });
    auto combine_pipeline = crd::make_graphics_pipeline(context, renderer, {
        .vertex = "data/shaders/combine.vert.spv",
        .fragment = "data/shaders/combine.frag.spv",
        .render_pass = &deferred_pass,
        .attributes = {},
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
    const auto nlights = 256;
    std::vector<PointLight> lights = {};
    lights.reserve(nlights);
    for (int i = 0; i < nlights; ++i) {
        lights.push_back({
            .position = glm::vec4(random(-24, 24), -0.4f, random(-24, 24), 0.0f),
            .falloff = glm::vec4(1.0f, 0.34f, 0.44f, 0.0f),
            .diffuse = glm::vec4(random(0, 1), random(0, 1), random(0, 1), 0.0f),
            .specular = glm::vec4(1.0f)
        });
    }
    std::vector<glm::vec4> light_colors;
    light_colors.reserve(lights.size());
    for (const auto& light : lights) {
        light_colors.emplace_back(light.diffuse);
    }
    auto black = crd::request_static_texture(context, "data/textures/black.png", crd::texture_srgb);
    std::vector<crd::Async<crd::StaticModel>> models;
    models.emplace_back(crd::request_static_model(context, "data/models/cube/cube.obj"));
    models.emplace_back(crd::request_static_model(context, "data/models/plane/plane.obj"));
    std::vector<crd::Async<crd::StaticModel>*> model_handles;
    model_handles.emplace_back(&models[0]);
    model_handles.emplace_back(&models[1]);
    Camera camera;
    std::vector<glm::mat4> transforms;
    transforms.reserve(nlights + 2);
    for (const auto& light : lights) {
        transforms.emplace_back(glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(light.position)), glm::vec3(0.1f)));
    }
    transforms.emplace_back(glm::mat4(1.0f));
    transforms.emplace_back(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.5f, 0.0f)));
    auto camera_buffer = crd::make_buffer(context, sizeof(glm::mat4), crd::uniform_buffer);
    auto model_buffer = crd::make_buffer(context, crd::size_bytes(transforms), crd::storage_buffer);
    auto light_color_buffer = crd::make_buffer(context, crd::size_bytes(lights), crd::storage_buffer);
    auto light_uniform_buffer = crd::make_buffer(context, sizeof(glm::vec4), crd::uniform_buffer);
    auto point_light_buffer = crd::make_buffer(context, crd::size_bytes(lights), crd::storage_buffer);
    auto directional_light_buffer = crd::make_buffer(context, sizeof(glm::mat4), crd::storage_buffer);
    auto main_set = crd::make_descriptor_set(context, main_pipeline.layout.descriptor[0]);
    auto light_set = crd::make_descriptor_set(context, light_pipeline.layout.descriptor[0]);
    auto gbuffer_set = crd::make_descriptor_set<1>(context, combine_pipeline.layout.descriptor[0]);
    auto light_data_set = crd::make_descriptor_set(context, combine_pipeline.layout.descriptor[1]);
    std::size_t frames = 0;
    double delta_time = 0, last_frame = 0, fps = 0;
    while (!window.is_closed()) {
        const auto [commands, image, index] = renderer.acquire_frame(context, window, swapchain);
        const auto scene = build_scene(model_handles, black->info());
        const auto current_frame = crd::time();
        ++frames;
        delta_time = current_frame - last_frame;
        last_frame = current_frame;
        fps += delta_time;
        if (fps >= 1.6) {
            crd::detail::log("Scene", crd::detail::severity_info, crd::detail::type_performance, "Average FPS: %lf ", 1 / (fps / frames));
            frames = 0;
            fps = 0;
        }
        model_buffer[index].resize(context, crd::size_bytes(transforms));
        point_light_buffer[index].resize(context, crd::size_bytes(lights));
        light_color_buffer[index].resize(context, crd::size_bytes(light_colors));
        model_buffer[index].write(transforms.data(), 0);
        camera_buffer[index].write(glm::value_ptr(camera.raw()), 0);
        point_light_buffer[index].write(lights.data(), 0, crd::size_bytes(lights));
        light_color_buffer[index].write(light_colors.data(), 0, crd::size_bytes(light_colors));
        light_uniform_buffer[index].write(&camera.position, 0, sizeof camera.position);
        main_set[index]
            .bind(context, main_pipeline.bindings["Uniforms"], camera_buffer[index].info())
            .bind(context, main_pipeline.bindings["Models"], model_buffer[index].info())
            .bind(context, main_pipeline.bindings["textures"], scene.descriptors);
        light_set[index]
            .bind(context, light_pipeline.bindings["Uniforms"], camera_buffer[index].info())
            .bind(context, light_pipeline.bindings["Models"], model_buffer[index].info())
            .bind(context, light_pipeline.bindings["Colors"], light_color_buffer[index].info());
        light_data_set[index]
            .bind(context, combine_pipeline.bindings["Uniforms"], light_uniform_buffer[index].info())
            // .bind(context, combine_pipeline.bindings["DirectionalLights"], directional_light_buffer[index].info())
            .bind(context, combine_pipeline.bindings["PointLights"], point_light_buffer[index].info());
        gbuffer_set
            .bind(context, combine_pipeline.bindings["i_position"], deferred_pass.image(1).sample(context.default_sampler))
            .bind(context, combine_pipeline.bindings["i_normal"], deferred_pass.image(2).sample(context.default_sampler))
            .bind(context, combine_pipeline.bindings["i_specular"], deferred_pass.image(3).sample(context.default_sampler))
            .bind(context, combine_pipeline.bindings["i_albedo"], deferred_pass.image(4).sample(context.default_sampler));
        commands
            .begin()
            .begin_render_pass(deferred_pass, 0)
            .set_viewport()
            .set_scissor()
            .bind_pipeline(main_pipeline)
            .bind_descriptor_set(0, main_set[index]);
        for (const auto& model : scene.models) {
            auto& raw_model = **model_handles[model.index];
            for (const auto& submesh : model.submeshes) {
                auto& raw_submesh = raw_model.submeshes[submesh.index];
                const std::uint32_t indices[] = {
                    model.index + nlights,
                    submesh.textures[0],
                    submesh.textures[1],
                    submesh.textures[2]
                };
                commands
                    .push_constants(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, indices, sizeof(indices))
                    .bind_static_mesh(*raw_submesh.mesh)
                    .draw_indexed(raw_submesh.indices, 1, 0, 0, 0);
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
            .draw_indexed(light_cube.indices, nlights, 0, 0, 0);
        commands
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
        renderer.present_frame(context, window, swapchain, commands, deferred_pass.stage);
        crd::poll_events();
        camera.update(window, delta_time);
    }
    context.graphics->wait_idle();
    crd::destroy_descriptor_set(context, gbuffer_set);
    crd::destroy_descriptor_set(context, light_data_set);
    crd::destroy_descriptor_set(context, light_set);
    crd::destroy_descriptor_set(context, main_set);
    crd::destroy_buffer(context, light_uniform_buffer);
    crd::destroy_buffer(context, light_color_buffer);
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