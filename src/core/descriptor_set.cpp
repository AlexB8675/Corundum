#include <corundum/core/descriptor_set.hpp>
#include <corundum/core/pipeline.hpp>
#include <corundum/core/context.hpp>
#include <corundum/core/buffer.hpp>
#include <corundum/core/image.hpp>

#include <corundum/detail/logger.hpp>
#include <corundum/detail/hash.hpp>

namespace crd {
    template <>
    crd_nodiscard crd_module DescriptorSet<1> make_descriptor_set(const Context& context, DescriptorSetLayout layout) noexcept {
        VkDescriptorSetAllocateInfo allocate_info;
        allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocate_info.pNext = nullptr;
        allocate_info.descriptorPool = context.descriptor_pool;
        allocate_info.descriptorSetCount = 1;
        allocate_info.pSetLayouts = &layout.handle;
        VkDescriptorSetVariableDescriptorCountAllocateInfo variable_count;
        if (layout.dynamic) {
            variable_count.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
            variable_count.pNext = nullptr;
            variable_count.descriptorSetCount = 1;
            variable_count.pDescriptorCounts = &layout.dyn_binds;
            allocate_info.pNext = &variable_count;
        }
        DescriptorSet<1> set;
        set.bound.reserve(128);
        crd_vulkan_check(vkAllocateDescriptorSets(context.device, &allocate_info, &set.handle));
        return set;
    }

    template <>
    crd_nodiscard crd_module DescriptorSet<in_flight> make_descriptor_set(const Context& context, DescriptorSetLayout layout) noexcept {
        DescriptorSet<in_flight> sets;
        for (auto& each : sets.handles) {
            each = make_descriptor_set<1>(context, layout);
        }
        return sets;
    }

    template <>
    crd_module void destroy_descriptor_set(const Context& context, DescriptorSet<1>& set) noexcept {
        crd_vulkan_check(vkFreeDescriptorSets(context.device, context.descriptor_pool, 1, &set.handle));
    }

    template <>
    crd_module void destroy_descriptor_set(const Context& context, DescriptorSet<in_flight>& sets) noexcept {
        for (auto& each : sets.handles) {
            destroy_descriptor_set(context, each);
        }
    }

    crd_module DescriptorSet<1>& DescriptorSet<1>::bind(const Context& context, const DescriptorBinding& binding, std::uint32_t offset, VkDescriptorBufferInfo buffer) noexcept {
        const auto binding_hash = detail::hash(0, binding);
        const auto descriptor_hash = detail::hash(0, buffer);
        const auto is_bound =
            std::find_if(bound.begin(), bound.end(), [=](const auto& each) {
                return each.binding == binding_hash &&
                       each.descriptor == descriptor_hash;
            });
        crd_unlikely_if(is_bound == bound.end()) {
            detail::log("Vulkan", detail::severity_info, detail::type_performance,
                        "updating buffer descriptor with binding: %d, handle: %p, range: %llu", binding.index, buffer.buffer, buffer.range);
            VkWriteDescriptorSet update;
            update.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            update.pNext = nullptr;
            update.dstSet = handle;
            update.dstBinding = binding.index;
            update.dstArrayElement = offset;
            update.descriptorCount = 1;
            update.descriptorType = binding.type;
            update.pImageInfo = nullptr;
            update.pBufferInfo = &buffer;
            update.pTexelBufferView = nullptr;
            vkUpdateDescriptorSets(context.device, 1, &update, 0, nullptr);
            bound.push_back({ binding_hash, descriptor_hash });
        }
        return *this;
    }

    crd_module DescriptorSet<1>& DescriptorSet<1>::bind(const Context& context, const DescriptorBinding& binding, std::uint32_t offset, VkDescriptorImageInfo image) noexcept {
        const auto binding_hash = detail::hash(0, binding);
        const auto descriptor_hash = detail::hash(0, image);
        const auto is_bound =
            std::find_if(bound.begin(), bound.end(), [=](const auto& each) {
                return each.binding == binding_hash &&
                       each.descriptor == descriptor_hash;
            });
        crd_unlikely_if(is_bound == bound.end()) {
            detail::log("Vulkan", detail::severity_info, detail::type_performance,
                        "updating image descriptor with binding: %d, handle: %p", binding.index, image.imageView);
            VkWriteDescriptorSet update;
            update.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            update.pNext = nullptr;
            update.dstSet = handle;
            update.dstBinding = binding.index;
            update.dstArrayElement = offset;
            update.descriptorCount = 1;
            update.descriptorType = binding.type;
            update.pImageInfo = &image;
            update.pBufferInfo = nullptr;
            update.pTexelBufferView = nullptr;
            vkUpdateDescriptorSets(context.device, 1, &update, 0, nullptr);
            bound.push_back({ binding_hash, descriptor_hash });
        }
        return *this;
    }

    crd_module DescriptorSet<1>& DescriptorSet<1>::bind(const Context& context, const DescriptorBinding& binding, std::uint32_t offset, const std::vector<VkDescriptorImageInfo>& images) noexcept {
        const auto binding_hash = detail::hash(0, binding);
        const auto descriptor_hash = detail::hash(0, images);
        const auto is_bound =
            std::find_if(bound.begin(), bound.end(), [=](const auto& each) {
                return each.binding == binding_hash &&
                       each.descriptor == descriptor_hash;
            });
        crd_unlikely_if(is_bound == bound.end()) {
            detail::log("Vulkan", detail::severity_info, detail::type_performance,
                        "updating image dynamic descriptor with binding: %d, images: %llu", binding.index, images.size());
            VkWriteDescriptorSet update;
            update.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            update.pNext = nullptr;
            update.dstSet = handle;
            update.dstBinding = binding.index;
            update.dstArrayElement = offset;
            update.descriptorCount = images.size();
            update.descriptorType = binding.type;
            update.pImageInfo = images.data();
            update.pBufferInfo = nullptr;
            update.pTexelBufferView = nullptr;
            vkUpdateDescriptorSets(context.device, 1, &update, 0, nullptr);
            bound.push_back({ binding_hash, descriptor_hash });
        }
        return *this;
    }

#if defined(crd_enable_raytracing)
    crd_module DescriptorSet<1>& DescriptorSet<1>::bind(const Context& context, const DescriptorBinding& binding, std::uint32_t offset, const std::vector<VkAccelerationStructureKHR>& tlases) noexcept {
        const auto binding_hash = detail::hash(0, binding);
        const auto descriptor_hash = detail::hash(0, tlases);
        const auto is_bound =
            std::find_if(bound.begin(), bound.end(), [=](const auto& each) {
                return each.binding == binding_hash &&
                       each.descriptor == descriptor_hash;
            });
        crd_unlikely_if(is_bound == bound.end()) {
            detail::log("Vulkan", detail::severity_info, detail::type_performance,
                        "updating TLAS dynamic descriptor with binding: %d, count: %llu", binding.index, tlases.size());
            VkWriteDescriptorSetAccelerationStructureKHR as_update;
            as_update.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
            as_update.pNext = nullptr;
            as_update.accelerationStructureCount = tlases.size();
            as_update.pAccelerationStructures = tlases.data();
            VkWriteDescriptorSet update;
            update.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            update.pNext = &as_update;
            update.dstSet = handle;
            update.dstBinding = binding.index;
            update.dstArrayElement = offset;
            update.descriptorCount = tlases.size();
            update.descriptorType = binding.type;
            update.pImageInfo = nullptr;
            update.pBufferInfo = nullptr;
            update.pTexelBufferView = nullptr;
            vkUpdateDescriptorSets(context.device, 1, &update, 0, nullptr);
            bound.push_back({ binding_hash, descriptor_hash });
        }
        return *this;
    }
#endif

    crd_module DescriptorSet<1>& DescriptorSet<1>::bind(const Context& context, const DescriptorBinding& binding, VkDescriptorBufferInfo buffer) noexcept {
        return bind(context, binding, 0, buffer);
    }

    crd_module DescriptorSet<1>& DescriptorSet<1>::bind(const Context& context, const DescriptorBinding& binding, VkDescriptorImageInfo image) noexcept {
        return bind(context, binding, 0, image);
    }

    crd_module DescriptorSet<1>& DescriptorSet<1>::bind(const Context& context, const DescriptorBinding& binding, const std::vector<VkDescriptorImageInfo>& images) noexcept {
        return bind(context, binding, 0, images);
    }

#if defined(crd_enable_raytracing)
    crd_module DescriptorSet<1>& DescriptorSet<1>::bind(const Context& context, const DescriptorBinding& binding, const std::vector<VkAccelerationStructureKHR>& tlases) noexcept {
        return bind(context, binding, 0, tlases);
    }
#endif

    crd_module DescriptorSet<in_flight>& DescriptorSet<in_flight>::bind(const Context& context, const DescriptorBinding& binding, VkDescriptorBufferInfo buffer) noexcept {
        for (auto& each : handles) {
            each.bind(context, binding, buffer);
        }
        return *this;
    }

    crd_module DescriptorSet<in_flight>& DescriptorSet<in_flight>::bind(const Context& context, const DescriptorBinding& binding, VkDescriptorImageInfo image) noexcept {
        for (auto& each : handles) {
            each.bind(context, binding, image);
        }
        return *this;
    }

    crd_module DescriptorSet<in_flight>& DescriptorSet<in_flight>::bind(const Context& context, const DescriptorBinding& binding, const std::vector<VkDescriptorImageInfo>& images) noexcept {
        for (auto& each : handles) {
            each.bind(context, binding, images);
        }
        return *this;
    }

#if defined(crd_enable_raytracing)
    crd_module DescriptorSet<in_flight>& DescriptorSet<in_flight>::bind(const Context& context, const DescriptorBinding& binding, const std::vector<VkAccelerationStructureKHR>& tlases) noexcept {
        for (auto& each : handles) {
            each.bind(context, binding, tlases);
        }
        return *this;
    }
#endif

    crd_module DescriptorSet<in_flight>& DescriptorSet<in_flight>::bind(const Context& context, const DescriptorBinding& binding, std::uint32_t offset, VkDescriptorBufferInfo buffer) noexcept {
        for (auto& each : handles) {
            each.bind(context, binding, offset, buffer);
        }
        return *this;
    }

    crd_module DescriptorSet<in_flight>& DescriptorSet<in_flight>::bind(const Context& context, const DescriptorBinding& binding, std::uint32_t offset, VkDescriptorImageInfo image) noexcept {
        for (auto& each : handles) {
            each.bind(context, binding, offset, image);
        }
        return *this;
    }

    crd_module DescriptorSet<in_flight>& DescriptorSet<in_flight>::bind(const Context& context, const DescriptorBinding& binding, std::uint32_t offset, const std::vector<VkDescriptorImageInfo>& images) noexcept {
        for (auto& each : handles) {
            each.bind(context, binding, offset, images);
        }
        return *this;
    }

#if defined(crd_enable_raytracing)
    crd_module DescriptorSet<in_flight>& DescriptorSet<in_flight>::bind(const Context& context, const DescriptorBinding& binding, std::uint32_t offset, const std::vector<VkAccelerationStructureKHR>& tlases) noexcept {
        for (auto& each : handles) {
            each.bind(context, binding, offset, tlases);
        }
        return *this;
    }
#endif

    crd_nodiscard crd_module const DescriptorSet<1>& DescriptorSet<in_flight>::operator [](std::size_t index) const noexcept {
        return handles[index];
    }

    crd_nodiscard crd_module DescriptorSet<1>& DescriptorSet<in_flight>::operator [](std::size_t index) noexcept {
        return handles[index];
    }
} // namespace crd
