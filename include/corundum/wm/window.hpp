#pragma once

#include <corundum/detail/forward.hpp>
#include <corundum/detail/macros.hpp>

#include <vulkan/vulkan.h>

#include <GLFW/glfw3.h>

#include <cstdint>

namespace crd {
    enum Keys {
        key_space      = GLFW_KEY_SPACE,
        key_a          = GLFW_KEY_A,
        key_b          = GLFW_KEY_B,
        key_c          = GLFW_KEY_C,
        key_d          = GLFW_KEY_D,
        key_e          = GLFW_KEY_E,
        key_f          = GLFW_KEY_F,
        key_g          = GLFW_KEY_G,
        key_h          = GLFW_KEY_H,
        key_i          = GLFW_KEY_I,
        key_j          = GLFW_KEY_J,
        key_k          = GLFW_KEY_K,
        key_l          = GLFW_KEY_L,
        key_m          = GLFW_KEY_M,
        key_n          = GLFW_KEY_N,
        key_o          = GLFW_KEY_O,
        key_p          = GLFW_KEY_P,
        key_q          = GLFW_KEY_Q,
        key_r          = GLFW_KEY_R,
        key_s          = GLFW_KEY_S,
        key_t          = GLFW_KEY_T,
        key_u          = GLFW_KEY_U,
        key_v          = GLFW_KEY_V,
        key_w          = GLFW_KEY_W,
        key_x          = GLFW_KEY_X,
        key_y          = GLFW_KEY_Y,
        key_z          = GLFW_KEY_Z,
        key_right      = GLFW_KEY_RIGHT,
        key_left       = GLFW_KEY_LEFT,
        key_down       = GLFW_KEY_DOWN,
        key_up         = GLFW_KEY_UP,
        key_left_shift = GLFW_KEY_LEFT_SHIFT
    };

    enum KeyState {
        key_released = GLFW_RELEASE,
        key_pressed  = GLFW_PRESS
    };

    struct Window {
        GLFWwindow* handle;
        std::uint32_t width;
        std::uint32_t height;

        crd_nodiscard crd_module bool     is_closed() const noexcept;
        crd_nodiscard crd_module KeyState key(Keys) const noexcept;
    };

    crd_nodiscard crd_module Window       make_window(std::uint32_t, std::uint32_t, const char*) noexcept;
    crd_nodiscard crd_module float        time() noexcept;
                  crd_module void         poll_events() noexcept;
    crd_nodiscard crd_module VkSurfaceKHR make_vulkan_surface(const Context&, const Window&) noexcept;
                  crd_module void         destroy_window(Window&) noexcept;
} // namespace crd
