#include <corundum/core/context.hpp>
#include <corundum/core/buffer.hpp>

#include <cstring>

namespace crd {
    template <>
    crd_nodiscard crd_module Buffer<1> make_buffer(const Context& context, std::size_t size, BufferType type) noexcept {
        Buffer<1> buffer;
        buffer.handle = make_static_buffer(context, {
            .flags = static_cast<VkBufferUsageFlags>(type),
            .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
            .capacity = size
        });
        buffer.size = size;
        return buffer;
    }

    template <>
    crd_nodiscard crd_module Buffer<in_flight> make_buffer(const Context& context, std::size_t size, BufferType type) noexcept {
        Buffer<in_flight> buffer;
        for (auto& handle : buffer.handles) {
            handle = make_buffer<1>(context, size, type);
        }
        return buffer;
    }

    template <>
    crd_module void destroy_buffer(const Context& context, Buffer<1>& buffer) noexcept {
        destroy_static_buffer(context, buffer.handle);
        buffer = {};
    }

    template <>
    crd_module void destroy_buffer(const Context& context, Buffer<in_flight>& buffer) noexcept {
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
        if (data) {
            std::memcpy(static_cast<char*>(handle.mapped) + offset, data, length);
        }
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

    template <>
    crd_module void resize_buffer(const Context& context, Buffer<1>& buffer, std::size_t new_size) noexcept {
        crd_likely_if(new_size == buffer.size) {
            return;
        }
        crd_unlikely_if(new_size >= buffer.handle.capacity) {
            auto old = buffer.handle;
            buffer.handle = make_static_buffer(context, {
                .flags = old.flags,
                .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                .capacity = new_size
            });
            std::memcpy(buffer.handle.mapped, old.mapped, buffer.size);
            destroy_static_buffer(context, old);
        }
        buffer.size = new_size;
    }

    template <>
    crd_module void resize_buffer(const Context& context, Buffer<in_flight>& buffer, std::size_t new_size) noexcept {
        for (auto& each : buffer.handles) {
            resize_buffer(context, each, new_size);
        }
    }

    template <>
    crd_module void shrink_buffer(const Context& context, Buffer<1>& buffer) noexcept {
        crd_unlikely_if(buffer.size < buffer.handle.capacity) {
            auto old = buffer.handle;
            buffer.handle = make_static_buffer(context, {
                .flags = old.flags,
                .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                .capacity = buffer.size
            });
            std::memcpy(buffer.handle.mapped, old.mapped, buffer.size);
            destroy_static_buffer(context, old);
        }
    }

    template <>
    crd_module void shrink_buffer(const Context& context, Buffer<in_flight>& buffer) noexcept {
        for (auto& each : buffer.handles) {
            shrink_buffer(context, each);
        }
    }
} // namespace crd
