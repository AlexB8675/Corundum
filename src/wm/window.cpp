#include <corundum/core/context.hpp>

#include <corundum/wm/window.hpp>

#include <GLFW/glfw3.h>

namespace crd {
    crd_nodiscard crd_module Window make_window(std::uint32_t width, std::uint32_t height, const char* title) noexcept {
        crd_assert(glfwInit(), "couldn't initialize GLFW");
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        const auto handle = glfwCreateWindow(width, height, title, nullptr, nullptr);
        crd_assert(handle, "cannot create window");

        Window window;
        window.handle = handle;
        window.width  = width;
        window.height = height;
        return window;
    }

    crd_nodiscard crd_module float time() noexcept {
        return glfwGetTime();
    }

    crd_module void poll_events() noexcept {
        glfwPollEvents();
    }

    crd_nodiscard crd_module VkSurfaceKHR make_vulkan_surface(const Context& context, const Window& window) noexcept {
        VkSurfaceKHR surface;
        crd_vulkan_check(glfwCreateWindowSurface(context.instance, window.handle, nullptr, &surface));
        return surface;
    }

    crd_module void destroy_window(Window& window) noexcept {
        glfwDestroyWindow(window.handle);
        window = {};
    }

    crd_nodiscard crd_module bool Window::is_closed() const noexcept {
        return glfwWindowShouldClose(handle);
    }

    crd_nodiscard crd_module KeyState Window::key(Keys key) const noexcept {
        return static_cast<KeyState>(glfwGetKey(handle, static_cast<int>(key)));
    }

    crd_nodiscard crd_module VkExtent2D Window::viewport() noexcept {
        int new_width, new_height;
        glfwGetFramebufferSize(handle, &new_width, &new_height);
        while (new_width == 0 || new_height == 0) {
            glfwGetFramebufferSize(handle, &new_width, &new_height);
            glfwWaitEvents();
        }
        return {
            width  = new_width,
            height = new_height
        };
    }
} // namespace crd
