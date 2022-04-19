#include <corundum/core/context.hpp>

#include <corundum/wm/window.hpp>

#include <Tracy.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <GLFW/glfw3.h>

namespace crd {
    crd_nodiscard crd_module Window make_window(std::uint32_t width, std::uint32_t height, const char* title) noexcept {
        crd_profile_scoped();
        spdlog::set_default_logger(spdlog::stdout_color_mt("core"));
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
        window.width = width;
        window.height = height;
        window.fullscreen = false;
        return window;
    }

    crd_nodiscard crd_module float current_time() noexcept {
        crd_profile_scoped();
        return glfwGetTime();
    }

    crd_module void poll_events() noexcept {
        crd_profile_scoped();
        glfwPollEvents();
    }

    crd_nodiscard crd_module VkSurfaceKHR make_vulkan_surface(const Context& context, const Window& window) noexcept {
        crd_profile_scoped();
        VkSurfaceKHR surface;
        crd_vulkan_check(glfwCreateWindowSurface(context.instance, window.handle, nullptr, &surface));
        return surface;
    }

    crd_module void destroy_window(Window& window) noexcept {
        crd_profile_scoped();
        glfwDestroyWindow(window.handle);
        window = {};
    }

    crd_nodiscard crd_module bool Window::is_closed() const noexcept {
        crd_profile_scoped();
        return glfwWindowShouldClose(handle);
    }

    crd_module void Window::close() const noexcept {
        crd_profile_scoped();
        glfwSetWindowShouldClose(handle, true);
    }

    crd_nodiscard crd_module KeyState Window::key(Key key) const noexcept {
        crd_profile_scoped();
        return static_cast<KeyState>(glfwGetKey(handle, static_cast<int>(key)));
    }

    crd_nodiscard crd_module VkExtent2D Window::viewport() noexcept {
        crd_profile_scoped();
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
            width = new_width,
            height = new_height
        };
    }

    crd_module void Window::toggle_fullscreen() noexcept {
        crd_profile_scoped();
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

    crd_module void Window::set_resize_callback(resize_callback_t&& callback) noexcept {
        crd_profile_scoped();
        resize_callback = std::move(callback);
        glfwSetWindowUserPointer(handle, this);
    }

    crd_module void Window::set_key_callback(key_callback_t&& callback) noexcept {
        crd_profile_scoped();
        key_callback = std::move(callback);
        glfwSetWindowUserPointer(handle, this);
        glfwSetKeyCallback(handle, [](GLFWwindow* l_handle, int key, int, int action, int) {
            static_cast<Window*>(glfwGetWindowUserPointer(l_handle))->key_callback(static_cast<Key>(key), static_cast<KeyState>(action));
        });
    }
} // namespace crd
