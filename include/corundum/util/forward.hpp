#pragma once

struct GLFWwindow;

namespace crd {
    namespace core {
        struct Context;
        struct Swapchain;
        struct Image;
        struct RenderPass;
        struct Pipeline;
        struct Queue;
        struct CommandBuffer;
        struct StaticBuffer;
        template <typename T>
        struct Async;
    } // namespace crd::core

    namespace wm {
        struct Window;
    } // namespace crd::wm
} // namespace crd
