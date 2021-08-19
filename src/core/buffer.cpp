#include <corundum/core/context.hpp>
#include <corundum/core/buffer.hpp>

#include <cstring>

namespace crd {
    template <>
    crd_nodiscard crd_module Buffer<1> make_buffer(const Context& context, std::size_t size, BufferType type) {
        Buffer<1> buffer;
        buffer.handle = make_static_buffer(context, {
            .flags = static_cast<VkBufferUsageFlags>(type),
            .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
            .capacity = size
        });
        buffer.size = 0;
        return buffer;
    }

    template <>
    crd_nodiscard crd_module Buffer<in_flight> make_buffer(const Context& context, std::size_t size, BufferType type) {
        Buffer buffer;
        for (auto& handle : buffer.handles) {
            handle = make_buffer<1>(context, size, type);
        }
        return buffer;
    }

    template <>
    crd_module void destroy_buffer(const Context& context, Buffer<1>& buffer) {
        destroy_static_buffer(context, buffer.handle);
        buffer = {};
    }

    template <>
    crd_module void destroy_buffer(const Context& context, Buffer<in_flight>& buffer) {
        for (auto& handle : buffer.handles) {
            destroy_buffer(context, handle);
        }
        buffer = {};
    }

    crd_nodiscard crd_module VkDescriptorBufferInfo Buffer<1>::info() const noexcept {
        return { handle.handle, 0, size };
    }

    crd_nodiscard crd_module std::size_t Buffer<1>::capacity() const noexcept {
        return handle.capacity;
    }

    crd_nodiscard crd_module const char* Buffer<1>::view() const noexcept {
        return static_cast<const char*>(handle.mapped);
    }

    crd_nodiscard crd_module char* Buffer<1>::raw() const noexcept {
        return static_cast<char*>(handle.mapped);
    }

    crd_module void Buffer<1>::write(const void* data, std::size_t offset) noexcept {
        write(data, offset, handle.capacity);
    }

    crd_module void Buffer<1>::write(const void* data, std::size_t offset, std::size_t length) noexcept {
        crd_assert(length + offset <= handle.capacity, "can't write past end pointer");
        size = length + offset;
        std::memcpy(static_cast<char*>(handle.mapped) + offset, data, length);
    }

    crd_module void Buffer<1>::resize(const Context& context, std::size_t new_size) noexcept {
        crd_likely_if(new_size == size) {
            return;
        }
        crd_unlikely_if(new_size >= handle.capacity) {
            auto old = handle;
            handle = make_static_buffer(context, {
                .flags = old.flags,
                .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                .capacity = new_size
            });
            std::memcpy(handle.mapped, old.mapped, size);
            destroy_static_buffer(context, old);
        }
        size = new_size;
    }

    crd_module Buffer<1>& Buffer<in_flight>::operator [](std::size_t index) noexcept {
        return handles[index];
    }

    crd_module void Buffer<in_flight>::write(const void* data, std::size_t offset) noexcept {
        for (auto& each : handles) {
            each.write(data, offset);
        }
    }

    crd_module void Buffer<in_flight>::write(const void* data, std::size_t offset, std::size_t length) noexcept {
        for (auto& each : handles) {
            each.write(data, offset, length);
        }
    }

    crd_module void Buffer<in_flight>::resize(const Context& context, std::size_t new_size) noexcept {
        for (auto& each : handles) {
            each.resize(context, new_size);
        }
    }
} // namespace crd
