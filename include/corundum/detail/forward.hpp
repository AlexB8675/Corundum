#pragma once

#include <corundum/detail/macros.hpp>

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
    struct ClearValue;
    struct GraphicsPipeline;
    struct ComputePipeline;
    struct Queue;
    struct CommandBuffer;
    struct Renderer;
    struct StaticBuffer;
    struct StaticMesh;
    struct StaticTexture;
    struct StaticModel;
    struct DescriptorBinding;
    struct DescriptorSetLayout;
#if defined(crd_enable_raytracing)
    struct AccelerationStructure;
#endif
    template <std::size_t>
    struct Buffer;
    template <std::size_t>
    struct DescriptorSet;
    template <typename T>
    struct Async;
    struct Window;
} // namespace crd
