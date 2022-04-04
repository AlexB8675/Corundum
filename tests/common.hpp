#ifndef CORUNDUM_TESTS_COMMON_HPP
#define CORUNDUM_TESTS_COMMON_HPP

#define max_shadow_cascades 16
#define max_directional_lights 4
#define max_lights_per_tile 512
#define shadow_cascades 4
#define tile_size 16

#if defined(crd_enable_raytracing)
    #include <corundum/core/acceleration_structure.hpp>
#endif
#include <corundum/core/descriptor_set.hpp>
#include <corundum/core/static_texture.hpp>
#include <corundum/core/static_model.hpp>
#include <corundum/core/static_mesh.hpp>
#include <corundum/core/render_pass.hpp>
#include <corundum/core/utilities.hpp>
#include <corundum/core/swapchain.hpp>
#include <corundum/core/constants.hpp>
#include <corundum/core/renderer.hpp>
#include <corundum/core/dispatch.hpp>
#include <corundum/core/pipeline.hpp>
#include <corundum/core/context.hpp>
#include <corundum/core/buffer.hpp>
#include <corundum/core/clear.hpp>
#include <corundum/core/async.hpp>

#include <corundum/detail/logger.hpp>

#include <corundum/wm/window.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>

#include <filesystem>
#include <random>
#include <vector>
#include <array>
#include <cmath>
#include <span>

struct Camera {
    glm::mat4 projection;
    glm::mat4 view;
    glm::vec3 position = { 3.5f, 3.5f,  0.0f };
    glm::vec3 front = { 0.0f, 0.0f, -1.0f };
    glm::vec3 up = { 0.0f, 1.0f,  0.0f };
    glm::vec3 right = { 0.0f, 0.0f,  0.0f };
    glm::vec3 world_up = { 0.0f, 1.0f,  0.0f };
    float yaw = -180.0f;
    float pitch = 0.0f;
    float aspect = 0.0f;
    const float near = 0.1f;
    const float far = 256.0f;

    void update(const crd::Window& window, double delta_time) noexcept {
        _process_keyboard(window, delta_time);
        aspect = window.width / (float)window.height;
        projection = glm::perspective(glm::radians(90.0f), aspect, near, far);
        projection[1][1] *= -1;
        const auto cos_pitch = std::cos(glm::radians(pitch));
        front = glm::normalize(glm::vec3{
            std::cos(glm::radians(yaw)) * cos_pitch,
            std::sin(glm::radians(pitch)),
            std::sin(glm::radians(yaw)) * cos_pitch
        });
        right = glm::normalize(glm::cross(front, world_up));
        up = glm::normalize(glm::cross(right, front));
        view = glm::lookAt(position, position + front, up);
    }

    glm::mat4 raw() const noexcept {
        return projection * view;
    }
private:
    void _process_keyboard(const crd::Window& window, double delta_time) noexcept {
        constexpr auto camera_speed = 7.5f;
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

struct Cascade {
    glm::mat4 pv;
    float level;
    float _0[3];
};

struct Model {
    struct Submesh {
        std::array<std::uint32_t, 3> textures;
        std::uint32_t index;
    };
    crd::Async<crd::StaticModel>* handle;
    std::vector<Submesh> submeshes;
    std::uint32_t transform;
    std::uint32_t instances;
};

struct Scene {
    std::vector<VkDescriptorImageInfo> descriptors;
    std::vector<glm::mat4> transforms;
    std::vector<Model> models;
};

struct DirectionalLight {
    glm::vec4 direction;
    glm::vec4 diffuse;
    glm::vec4 specular;
};

struct PointLight {
    glm::vec4 position;
    glm::vec4 diffuse;
    glm::vec4 specular;
    glm::vec4 falloff;
};

struct Rotation {
    glm::vec3 axis;
    float angle;
};

struct Transform {
    glm::vec3 position;
    Rotation rotation;
    glm::vec3 scale;
};

struct Draw {
    crd::Async<crd::StaticModel>* model;
    std::vector<Transform> transforms;
};

static inline float random(float min, float max) {
    static std::random_device device;
    static std::mt19937 engine(device());
    return std::uniform_real_distribution<float>(min, max)(engine);
}

static inline Scene build_scene(std::span<Draw> draws, VkDescriptorImageInfo fallback) noexcept {
    Scene scene;
    scene.descriptors = { fallback };
    std::unordered_map<void*, std::uint32_t> cache;
    std::size_t t_size = 0;
    for (const auto& [_, transforms] : draws) {
        t_size += transforms.size();
    }
    scene.transforms.reserve(t_size);
    std::size_t offset = 0;
    for (std::uint32_t i = 0; auto [model, transforms] : draws) {
        crd_likely_if(model->is_ready()) {
            const auto submeshes_size = (*model)->submeshes.size();
            auto& handle = scene.models.emplace_back();
            scene.descriptors.reserve(scene.descriptors.size() + submeshes_size * 3);
            cache.reserve(cache.size() + submeshes_size * 3 + transforms.size());
            handle.handle = model;
            handle.transform = offset;
            handle.instances = transforms.size();
            handle.submeshes.reserve(submeshes_size);
            for (std::uint32_t j = 0; auto& submesh : (*model)->submeshes) {
                crd_likely_if(submesh.mesh.is_ready()) {
                    std::array<std::uint32_t, 3> indices = {};
                    const auto emplace_descriptor = [&](crd::Async<crd::StaticTexture>* texture, std::uint32_t which) {
                        crd_likely_if(texture) {
                            auto& cached = cache[texture];
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
        for (const auto& transform : transforms) {
            auto result = glm::mat4(1.0f);
            result = glm::translate(result, transform.position);
            crd_unlikely_if(std::fpclassify(transform.rotation.angle) != FP_ZERO) {
                result = glm::rotate(result, transform.rotation.angle, transform.rotation.axis);
            }
            result = glm::scale(result, transform.scale);
            scene.transforms.emplace_back(result);
        }
        offset += transforms.size();
        i++;
    }
    return scene;
}

static inline std::array<Cascade, shadow_cascades> calculate_cascades(const Camera& camera, glm::vec3 light_pos) noexcept {
    std::array<Cascade, shadow_cascades> cascades;
    const auto calculate_cascade = [&camera, &light_pos](float near, float far) {
        const auto perspective = glm::perspective(glm::radians(90.0f), camera.aspect, near, far);
        glm::vec4 corners[8];
        { // Calculate frustum corners
            const auto inverse = glm::inverse(perspective * camera.view);
            std::uint32_t offset = 0;
            for (std::uint32_t x = 0; x < 2; ++x) {
                for (std::uint32_t y = 0; y < 2; ++y) {
                    for (std::uint32_t z = 0; z < 2; ++z) {
                        const auto v_world = inverse * glm::vec4(
                            2.0f * (float)x - 1.0f,
                            2.0f * (float)y - 1.0f,
                            2.0f * (float)z - 1.0f,
                            1.0f);
                        corners[offset++] = v_world / v_world.w;
                    }
                }
            }
        }
        glm::mat4 light_view;
        { // Calculate light view matrix
            auto center = glm::vec3(0.0f);
            for (const auto& corner : corners) {
                center += glm::vec3(corner);
            }
            center /= 8.0f;
            light_pos.x += 0.01f;
            light_pos.z += 0.01f;
            const auto light_dir = glm::normalize(light_pos);
            const auto eye = center + light_dir;
            light_view = glm::lookAt(eye, center, { 0.0f, 1.0f, 0.0f });
        }

        glm::mat4 light_proj;
        {
            float min_x = (std::numeric_limits<float>::max)();
            float max_x = (std::numeric_limits<float>::min)();
            float min_y = min_x;
            float max_y = max_x;
            float min_z = min_x;
            float max_z = max_x;
            for (const auto& corner : corners) {
                const auto trf = light_view * corner;
                min_x = std::min(min_x, trf.x);
                max_x = std::max(max_x, trf.x);
                min_y = std::min(min_y, trf.y);
                max_y = std::max(max_y, trf.y);
                min_z = std::min(min_z, trf.z);
                max_z = std::max(max_z, trf.z);
            }
            constexpr auto z_mult = 10.0f;
            if (min_z < 0) {
                min_z *= z_mult;
            } else {
                min_z /= z_mult;
            }
            if (max_z < 0) {
                max_z /= z_mult;
            } else {
                max_z *= z_mult;
            }

            light_proj = glm::ortho(min_x, max_x, min_y, max_y, min_z, max_z);
        }
        return light_proj * light_view;
    };
    const float cascade_levels[shadow_cascades] = {
        camera.far / 15.0f,
        camera.far / 7.5f,
        camera.far / 2.5f,
        camera.far,
    };
    for (std::size_t i = 0; i < shadow_cascades; ++i) {
        float near;
        float far;
        crd_unlikely_if(i == 0) {
            near = camera.near;
            far = cascade_levels[i];
        } else {
            near = cascade_levels[i - 1];
            far = cascade_levels[i];
        }
        cascades[i] = { calculate_cascade(near, far), far };
    }
    return cascades;
}

template <typename T>
static inline T reload_pipelines(const crd::Context& context, crd::Renderer& renderer, T& pipeline, typename T::CreateInfo&& info) noexcept {
    switch (pipeline.type) {
        case T::type_graphics:
        case T::type_raytracing: {
            context.graphics->wait_idle();
        } break;

        case T::type_compute: {
            context.compute->wait_idle();
        } break;
    }
    crd::destroy_pipeline(context, pipeline);
    return crd::make_pipeline(context, renderer, std::move(info));
}

#endif //CORUNDUM_TESTS_COMMON_HPP
