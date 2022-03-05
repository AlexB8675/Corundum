#pragma once

#include <corundum/core/static_buffer.hpp>
#include <corundum/core/constants.hpp>

#include <corundum/detail/forward.hpp>
#include <corundum/detail/macros.hpp>

#include <array>

namespace crd {
    enum BufferType {
        uniform_buffer = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        storage_buffer = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
#if defined(crd_enable_raytracing)
        acceleration_structure_instance = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
#endif
    };

    enum MemoryUsage {
        device_local = VMA_MEMORY_USAGE_GPU_ONLY,
        host_visible = VMA_MEMORY_USAGE_CPU_TO_GPU,
        host_only = VMA_MEMORY_USAGE_CPU_ONLY
    };

    template <>
    struct Buffer<1> {
        StaticBuffer handle;
        std::size_t size;

        crd_nodiscard crd_module VkDescriptorBufferInfo info() const noexcept;
        crd_nodiscard crd_module std::size_t            capacity() const noexcept;
        crd_nodiscard crd_module const char*            view() const noexcept;
        crd_nodiscard crd_module char*                  raw() const noexcept;
                      crd_module void                   write(const void*, std::size_t) noexcept;
                      crd_module void                   write(const void*, std::size_t, std::size_t) noexcept;
    };

    template <>
    struct Buffer<in_flight> {
        struct CreateInfo {
            BufferType type;
            MemoryUsage usage;
            std::size_t capacity;
        };
        std::array<Buffer<1>, in_flight> handles;

                      crd_module void       write(const void*, std::size_t) noexcept;
                      crd_module void       write(const void*, std::size_t, std::size_t) noexcept;

        crd_nodiscard crd_module Buffer<1>& operator [](std::size_t) noexcept;
   };

    template <std::size_t N = in_flight> crd_nodiscard crd_module Buffer<N> make_buffer(const Context&, Buffer<in_flight>::CreateInfo&&) noexcept;
    template <std::size_t N = in_flight>               crd_module void      destroy_buffer(const Context&, Buffer<N>&) noexcept;
    template <std::size_t N = in_flight>               crd_module void      resize_buffer(const Context&, Buffer<N>&, std::size_t) noexcept;
    template <std::size_t N = in_flight>               crd_module void      shrink_buffer(const Context&, Buffer<N>&) noexcept;
} // namespace crd
