#include <corundum/core/descriptor_set.hpp>
#include <corundum/core/pipeline.hpp>
#include <corundum/core/context.hpp>
#include <corundum/core/buffer.hpp>

#include <corundum/util/hash.hpp>

namespace crd::core {
    template <>
    crd_nodiscard crd_module DescriptorSet<1> make_descriptor_set(const Context& context, DescriptorSetLayout layout) noexcept {
        VkDescriptorSetAllocateInfo allocate_info;
        allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocate_info.pNext = nullptr;
        allocate_info.descriptorPool = context.descriptor_pool;
        allocate_info.descriptorSetCount = 1;
        allocate_info.pSetLayouts = &layout.handle;

        constexpr auto count = 4096u;
        VkDescriptorSetVariableDescriptorCountAllocateInfo variable_count;
        if (layout.dynamic) {
            variable_count.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
            variable_count.pNext = nullptr;
            variable_count.descriptorSetCount = 1;
            variable_count.pDescriptorCounts = &count;
            allocate_info.pNext = &variable_count;
        }

        DescriptorSet<1> set;
        set.bound.reserve(128);
        crd_vulkan_check(vkAllocateDescriptorSets(context.device, &allocate_info, &set.handle));
        return set;
    }

    template <>
    crd_nodiscard crd_module DescriptorSet<in_flight> make_descriptor_set(const Context& context, DescriptorSetLayout layout) noexcept {
        DescriptorSet sets;
        for (auto& each : sets.handles) {
            each = make_descriptor_set<1>(context, layout);
        }
        return sets;
    }

    template <>
    crd_module void destroy_descriptor_set(const Context& context, DescriptorSet<1>& set) noexcept {
        vkFreeDescriptorSets(context.device, context.descriptor_pool, 1, &set.handle);
    }

    template <>
    crd_module void destroy_descriptor_set(const Context& context, DescriptorSet<in_flight>& sets) noexcept {
        for (auto& each : sets.handles) {
            destroy_descriptor_set(context, each);
        }
    }

    crd_module DescriptorSet<1>& DescriptorSet<1>::bind(const Context& context, const DescriptorBinding& binding, VkDescriptorBufferInfo buffer) noexcept {
        const auto  buffer_hash      = util::hash(0, buffer);
        const auto  binding_hash     = util::hash(0, binding);
              auto& bound_descriptor = bound[binding_hash];
        crd_unlikely_if(bound_descriptor != buffer_hash) {
            VkWriteDescriptorSet update;
            update.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            update.pNext = nullptr;
            update.dstSet = handle;
            update.dstBinding = binding.index;
            update.dstArrayElement = 0;
            update.descriptorCount = 1;
            update.descriptorType = binding.type;
            update.pImageInfo = nullptr;
            update.pBufferInfo = &buffer;
            update.pTexelBufferView = nullptr;
            vkUpdateDescriptorSets(context.device, 1, &update, 0, nullptr);
            bound_descriptor = buffer_hash;
        }
        return *this;
    }

    crd_module DescriptorSet<1>& DescriptorSet<1>::bind(const Context& context, const DescriptorBinding& binding, VkDescriptorImageInfo image) noexcept {
        const auto  image_hash       = util::hash(0, image);
        const auto  binding_hash     = util::hash(0, binding);
              auto& bound_descriptor = bound[binding_hash];
        crd_unlikely_if(bound_descriptor != image_hash) {
            VkWriteDescriptorSet update;
            update.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            update.pNext = nullptr;
            update.dstSet = handle;
            update.dstBinding = binding.index;
            update.dstArrayElement = 0;
            update.descriptorCount = 1;
            update.descriptorType = binding.type;
            update.pImageInfo = &image;
            update.pBufferInfo = nullptr;
            update.pTexelBufferView = nullptr;
            vkUpdateDescriptorSets(context.device, 1, &update, 0, nullptr);
            bound_descriptor = image_hash;
        }
        return *this;
    }

    crd_nodiscard crd_module const DescriptorSet<1>& DescriptorSet<in_flight>::operator [](std::size_t index) const noexcept {
        return handles[index];
    }

    crd_nodiscard crd_module DescriptorSet<1>& DescriptorSet<in_flight>::operator [](std::size_t index) noexcept {
        return handles[index];
    }
} // namespace crd::core