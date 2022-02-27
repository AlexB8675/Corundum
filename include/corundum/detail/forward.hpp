#pragma once

#include <cstddef>

struct GLFWwindow;

namespace ftl {
    class TaskScheduler;
} // namespace ftl

namespace crd {
    struct Context;
    struct Swapchain;
    struct Image;
    struct Framebuffer;
    struct RenderPass;
    struct Pipeline;
    struct GraphicsPipeline;
    struct ComputePipeline;
    struct Queue;
    struct CommandBuffer;
    struct Renderer;
    struct StaticBuffer;
    struct StaticMesh;
    struct StaticTexture;
    struct DescriptorBinding;
    struct DescriptorSetLayout;
    template <std::size_t>
    struct Buffer;
    template <std::size_t>
    struct DescriptorSet;
    template <typename T>
    struct Async;
    struct Window;
} // namespace crd
