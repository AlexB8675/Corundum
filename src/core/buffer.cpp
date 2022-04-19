#include <corundum/core/context.hpp>
#include <corundum/core/buffer.hpp>

#include <Tracy.hpp>

#include <cstring>

namespace crd {
    template <>
    crd_nodiscard crd_module Buffer<1> make_buffer(const Context& context, Buffer<in_flight>::CreateInfo&& info) noexcept {
        crd_profile_scoped();
        Buffer<1> buffer;
        buffer.handle = make_static_buffer(context, {
            .flags = static_cast<VkBufferUsageFlags>(info.type),
            .usage = static_cast<VmaMemoryUsage>(info.usage),
            .capacity = info.capacity
        });
        buffer.size = info.capacity;
        return buffer;
    }

    template <>
    crd_nodiscard crd_module Buffer<in_flight> make_buffer(const Context& context, Buffer<in_flight>::CreateInfo&& info) noexcept {
        crd_profile_scoped();
        Buffer<in_flight> buffer;
        for (auto& handle : buffer.handles) {
            handle = make_buffer<1>(context, std::move(info));
        }
        return buffer;
    }

    crd_module void Buffer<1>::destroy() noexcept {
        crd_profile_scoped();
        handle.destroy();
        *this = {};
    }

    crd_module void Buffer<in_flight>::destroy() noexcept {
        crd_profile_scoped();
        for (auto& each : handles) {
            each.destroy();
        }
        *this = {};
    }

    crd_nodiscard crd_module VkDescriptorBufferInfo Buffer<1>::info() const noexcept {
        crd_profile_scoped();
        return { handle.handle, 0, size };
    }

    crd_nodiscard crd_module std::size_t Buffer<1>::capacity() const noexcept {
        crd_profile_scoped();
        return handle.capacity;
    }

    crd_nodiscard crd_module const char* Buffer<1>::view() const noexcept {
        crd_profile_scoped();
        return static_cast<const char*>(handle.mapped);
    }

    crd_nodiscard crd_module char* Buffer<1>::raw() const noexcept {
        crd_profile_scoped();
        return static_cast<char*>(handle.mapped);
    }

    crd_module void Buffer<1>::write(const void* data) noexcept {
        crd_profile_scoped();
        crd_likely_if(data) {
            write(data, handle.capacity, 0);
        }
    }

    crd_module void Buffer<1>::write(const void* data, std::size_t length) noexcept {
        crd_profile_scoped();
        crd_likely_if(data) {
            write(data, length, 0);
        }
    }

    crd_module void Buffer<1>::write(const void* data, std::size_t length, std::size_t offset) noexcept {
        crd_profile_scoped();
        resize(length + offset);
        crd_likely_if(data) {
            std::memcpy(static_cast<char*>(handle.mapped) + offset, data, length);
        }
    }

    crd_module void Buffer<1>::shrink() noexcept {
        crd_profile_scoped();
        const auto context = handle.context;
        crd_unlikely_if(size < handle.capacity) {
            auto old = handle;
            handle = make_static_buffer(*context, {
                .flags = old.flags,
                .usage = old.usage,
                .capacity = size
            });
            std::memcpy(handle.mapped, old.mapped, size);
            old.destroy();
        }
    }

    crd_module void Buffer<1>::resize(std::size_t new_size) noexcept {
        crd_profile_scoped();
        const auto* context = handle.context;
        crd_likely_if(new_size == size) {
            return;
        }
        crd_unlikely_if(new_size >= handle.capacity) {
            auto old = handle;
            handle = make_static_buffer(*context, {
                .flags = old.flags,
                .usage = old.usage,
                .capacity = new_size
            });
            crd_likely_if(old.mapped) {
                std::memcpy(handle.mapped, old.mapped, size);
            }
            old.destroy();
        }
        size = new_size;
    }

    crd_module Buffer<1>& Buffer<in_flight>::operator [](std::size_t index) noexcept {
        crd_profile_scoped();
        return handles[index];
    }

    crd_module void Buffer<in_flight>::write(const void* data) noexcept {
        crd_profile_scoped();
        for (auto& each : handles) {
            each.write(data);
        }
    }

    crd_module void Buffer<in_flight>::write(const void* data, std::size_t length) noexcept {
        crd_profile_scoped();
        for (auto& each : handles) {
            each.write(data, length);
        }
    }

    crd_module void Buffer<in_flight>::write(const void* data, std::size_t length, std::size_t offset) noexcept {
        crd_profile_scoped();
        for (auto& each : handles) {
            each.write(data, length, offset);
        }
    }

    crd_module void Buffer<in_flight>::shrink() noexcept {
        crd_profile_scoped();
        for (auto& each : handles) {
            each.shrink();
        }
    }

    crd_module void Buffer<in_flight>::resize(std::size_t new_size) noexcept {
        crd_profile_scoped();
        for (auto& each : handles) {
            each.resize(new_size);
        }
    }
} // namespace crd
