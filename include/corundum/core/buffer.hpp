#pragma once

#include <corundum/core/static_buffer.hpp>
#include <corundum/core/constants.hpp>

#include <corundum/detail/forward.hpp>
#include <corundum/detail/macros.hpp>

#include <array>

namespace crd {
    enum BufferType {
        uniform_buffer = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        storage_buffer = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
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
                      crd_module void                   resize(const Context&, std::size_t) noexcept;
    };

    template <>
    struct Buffer<in_flight> {
        std::array<Buffer<1>, in_flight> handles;

                      crd_module void       write(const void*, std::size_t) noexcept;
                      crd_module void       write(const void*, std::size_t, std::size_t) noexcept;
                      crd_module void       resize(const Context&, std::size_t) noexcept;

        crd_nodiscard crd_module Buffer<1>& operator [](std::size_t) noexcept;
   };

    template <std::size_t N = in_flight> crd_nodiscard crd_module Buffer<N> make_buffer(const Context&, std::size_t, BufferType);
    template <std::size_t N = in_flight>               crd_module void      destroy_buffer(const Context&, Buffer<N>&);
} // namespace crd
