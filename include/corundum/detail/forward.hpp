#pragma once

#include <corundum/core/constants.hpp>

#include <cstddef>

struct GLFWwindow;

namespace ftl {
    class TaskScheduler;
} // namespace ftl

namespace crd {
    struct Context;
    struct Swapchain;
    struct Image;
    struct RenderPass;
    struct Pipeline;
    struct Queue;
    struct CommandBuffer;
    struct Renderer;
    struct StaticBuffer;
    struct StaticMesh;
    struct StaticTexture;
    struct DescriptorBinding;
    struct DescriptorSetLayout;
    template <std::size_t = in_flight>
    struct Buffer;
    template <std::size_t = in_flight>
    struct DescriptorSet;
    template <typename T>
    struct Async;
    struct Window;
} // namespace crd
