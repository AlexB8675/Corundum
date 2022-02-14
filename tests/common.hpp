#ifndef CORUNDUM_TESTS_COMMON_HPP
#define CORUNDUM_TESTS_COMMON_HPP

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
#include <cmath>
#include <span>

struct Camera {
    glm::mat4 projection;
    glm::mat4 view;
    glm::vec3 position = { 3.5f, 3.5f,  0.0f };
    glm::vec3 front =    { 0.0f, 0.0f, -1.0f };
    glm::vec3 up =       { 0.0f, 1.0f,  0.0f };
    glm::vec3 right =    { 0.0f, 0.0f,  0.0f };
    glm::vec3 world_up = { 0.0f, 1.0f,  0.0f };
    float yaw = -180.0f;
    float pitch = 0.0f;
    float aspect = 0.0f;
    const float near = 0.1f;
    const float far = 150.0f;

    void update(const crd::Window& window, double delta_time) noexcept {
        _process_keyboard(window, delta_time);
        aspect = window.width / (float)window.height;
        projection = glm::perspective(glm::radians(60.0f), aspect, near, far);
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
        constexpr auto camera_speed = 5.0f;
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
    glm::vec4 falloff;
    glm::vec4 diffuse;
    glm::vec4 specular;
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

static inline Scene build_scene(std::span<Draw> models, VkDescriptorImageInfo fallback) noexcept {
    Scene scene;
    scene.descriptors = { fallback };
    std::unordered_map<void*, std::uint32_t> cache;
    std::size_t t_size = 0;
    for (const auto& [_, transforms] : models) {
        t_size += transforms.size();
    }
    scene.transforms.reserve(t_size);
    std::size_t offset = 0;
    for (std::uint32_t i = 0; auto [model, transforms] : models) {
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

#endif //CORUNDUM_TESTS_COMMON_HPP
