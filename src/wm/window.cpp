#include <corundum/core/context.hpp>

#include <corundum/wm/window.hpp>

#include <GLFW/glfw3.h>

namespace crd {
    crd_nodiscard crd_module Window make_window(std::uint32_t width, std::uint32_t height, const char* title) noexcept {
        crd_assert(glfwInit(), "couldn't initialize GLFW");
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        const auto handle = glfwCreateWindow(width, height, title, nullptr, nullptr);
        crd_assert(handle, "cannot create window");
        const auto video = glfwGetVideoMode(glfwGetPrimaryMonitor());
        glfwSetWindowPos(
            handle,
            video->width / 2 - width / 2,
            video->height / 2 - height / 2);
        Window window;
        window.handle = handle;
        window.width  = width;
        window.height = height;
        window.fullscreen = false;
        return window;
    }

    crd_nodiscard crd_module float current_time() noexcept {
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

    crd_module void Window::close() const noexcept {
        glfwSetWindowShouldClose(handle, true);
    }

    crd_nodiscard crd_module KeyState Window::key(Key key) const noexcept {
        return static_cast<KeyState>(glfwGetKey(handle, static_cast<int>(key)));
    }

    crd_nodiscard crd_module VkExtent2D Window::viewport() noexcept {
        int new_width, new_height;
        glfwGetFramebufferSize(handle, &new_width, &new_height);
        while (new_width == 0 || new_height == 0) {
            glfwWaitEvents();
            glfwGetFramebufferSize(handle, &new_width, &new_height);
        }
        if (fullscreen) {
            return { width, height };
        }
        return {
            width  = new_width,
            height = new_height
        };
    }

    crd_module void Window::toggle_fullscreen() noexcept {
        static int old_width = width;
        static int old_height = height;
        const auto monitor = glfwGetPrimaryMonitor();
        const auto video = glfwGetVideoMode(monitor);
        const auto next = glfwGetWindowMonitor(handle) ? nullptr : monitor;
        int x_pos = video->width / 2 - old_width / 2;
        int y_pos = video->height / 2 - old_height / 2;
        if (next) {
            x_pos = 0;
            y_pos = 0;
            width = video->width;
            height = video->height;
        } else {
            width = old_width;
            height = old_height;
            old_width = width;
            old_height = height;
        }
        fullscreen = !fullscreen;
        glfwSetWindowMonitor(
            handle, next,
            x_pos, y_pos,
            width, height,
            GLFW_DONT_CARE);
    }

    void Window::set_key_callback(Window::key_callback_t&& callback) noexcept {
        key_callback = std::move(callback);
        glfwSetWindowUserPointer(handle, this);
        glfwSetKeyCallback(handle, [](GLFWwindow* l_handle, int key, int, int action, int) {
            static_cast<Window*>(glfwGetWindowUserPointer(l_handle))->key_callback(static_cast<Key>(key), static_cast<KeyState>(action));
        });
    }
} // namespace crd
