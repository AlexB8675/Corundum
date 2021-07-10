#include <corundum/core/context.hpp>

#include <corundum/wm/window.hpp>

#include <GLFW/glfw3.h>

namespace crd::wm {
    crd_nodiscard crd_module Window make_window(std::uint32_t width, std::uint32_t height, const char* title) noexcept {
        crd_assert(glfwInit(), "couldn't initialize GLFW");

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, false);
        const auto handle = glfwCreateWindow(width, height, title, nullptr, nullptr);
        crd_assert(handle, "cannot create window");

        Window window;
        window.handle = handle;
        window.width  = width;
        window.height = height;
        return window;
    }

    crd_nodiscard crd_module bool is_closed(const Window& window) noexcept {
        return glfwWindowShouldClose(window.handle);
    }

    crd_module void poll_events() noexcept {
        glfwPollEvents();
    }

    crd_nodiscard crd_module VkSurfaceKHR make_vulkan_surface(const core::Context& context, const Window& window) noexcept {
        VkSurfaceKHR surface;
        crd_vulkan_check(glfwCreateWindowSurface(context.instance, window.handle, nullptr, &surface));
        return surface;
    }
} // namespace crd::wm
