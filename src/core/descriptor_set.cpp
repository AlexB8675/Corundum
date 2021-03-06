#include <corundum/core/acceleration_structure.hpp>
#include <corundum/core/descriptor_set.hpp>
#include <corundum/core/pipeline.hpp>
#include <corundum/core/context.hpp>
#include <corundum/core/buffer.hpp>
#include <corundum/core/image.hpp>

#include <corundum/detail/hash.hpp>

#include <spdlog/spdlog.h>

#if defined(crd_enable_profiling)
    #include <Tracy.hpp>
#endif

namespace crd {
    template <>
    crd_nodiscard crd_module DescriptorSet<1> make_descriptor_set(const Context& context, DescriptorSetLayout layout) noexcept {
        crd_profile_scoped();
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
        set.context = &context;
        set.bound.reserve(128);
        crd_vulkan_check(vkAllocateDescriptorSets(context.device, &allocate_info, &set.handle));
        return set;
    }

    template <>
    crd_nodiscard crd_module DescriptorSet<in_flight> make_descriptor_set(const Context& context, DescriptorSetLayout layout) noexcept {
        crd_profile_scoped();
        DescriptorSet<in_flight> sets;
        for (auto& each : sets.handles) {
            each = make_descriptor_set<1>(context, layout);
        }
        return sets;
    }

    crd_module DescriptorSet<1>& DescriptorSet<1>::bind(const DescriptorBinding& binding, std::uint32_t offset, VkDescriptorBufferInfo buffer) noexcept {
        crd_profile_scoped();
        const auto binding_hash = dtl::hash(0, binding);
        const auto descriptor_hash = dtl::hash(0, buffer);
        const auto is_bound =
            std::find_if(bound.begin(), bound.end(), [=](const auto& each) {
                return each.binding == binding_hash;
            });
        const auto found_binding = is_bound != bound.end();
        crd_unlikely_if(!found_binding || is_bound->descriptor != descriptor_hash) {
            spdlog::info("updating buffer descriptor with binding: {}, handle: {}, range: {}",
                         binding.index, (const void*)buffer.buffer, buffer.range);
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
            vkUpdateDescriptorSets(context->device, 1, &update, 0, nullptr);
            if (!found_binding) {
                bound.push_back({ binding_hash, descriptor_hash });
            } else {
                is_bound->descriptor = descriptor_hash;
            }
        }
        return *this;
    }

    crd_module DescriptorSet<1>& DescriptorSet<1>::bind(const DescriptorBinding& binding, std::uint32_t offset, VkDescriptorImageInfo image) noexcept {
        crd_profile_scoped();
        const auto binding_hash = dtl::hash(0, binding);
        const auto descriptor_hash = dtl::hash(0, image);
        const auto is_bound =
            std::find_if(bound.begin(), bound.end(), [=](const auto& each) {
                return each.binding == binding_hash;
            });
        const auto found_binding = is_bound != bound.end();
        crd_unlikely_if(!found_binding || is_bound->descriptor != descriptor_hash) {
            spdlog::info("updating image descriptor with binding: {}, handle: {}",
                         binding.index, (const void*)image.imageView);
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
            vkUpdateDescriptorSets(context->device, 1, &update, 0, nullptr);
            crd_likely_if(found_binding) {
                is_bound->descriptor = descriptor_hash;
            } else {
                bound.push_back({ binding_hash, descriptor_hash });
            }
        }
        return *this;
    }

#if defined(crd_enable_raytracing)
    crd_module DescriptorSet<1>& DescriptorSet<1>::bind(const DescriptorBinding& binding, std::uint32_t offset, const AccelerationStructure& tlas) noexcept {
        crd_profile_scoped();
        const auto binding_hash = dtl::hash(0, binding);
        const auto descriptor_hash = dtl::hash(0, tlas);
        const auto is_bound =
            std::find_if(bound.begin(), bound.end(), [=](const auto& each) {
                return each.binding == binding_hash;
            });
        const auto found_binding = is_bound != bound.end();
        crd_unlikely_if(!found_binding || is_bound->descriptor != descriptor_hash) {
            spdlog::info("updating TLAS descriptor with binding: {}, handle: {}",
                         binding.index, (const void*)tlas.handle);
            VkWriteDescriptorSetAccelerationStructureKHR as_update;
            as_update.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
            as_update.pNext = nullptr;
            as_update.accelerationStructureCount = 1;
            as_update.pAccelerationStructures = &tlas.handle;
            VkWriteDescriptorSet update;
            update.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            update.pNext = &as_update;
            update.dstSet = handle;
            update.dstBinding = binding.index;
            update.dstArrayElement = offset;
            update.descriptorCount = 1;
            update.descriptorType = binding.type;
            update.pImageInfo = nullptr;
            update.pBufferInfo = nullptr;
            update.pTexelBufferView = nullptr;
            vkUpdateDescriptorSets(context->device, 1, &update, 0, nullptr);
            if (found_binding) {
                is_bound->descriptor = descriptor_hash;
            } else {
                bound.push_back({ binding_hash, descriptor_hash });
            }
        }
        return *this;
    }
#endif

    crd_module DescriptorSet<1>& DescriptorSet<1>::bind(const DescriptorBinding& binding, std::uint32_t offset, const std::vector<VkDescriptorImageInfo>& images) noexcept {
        crd_profile_scoped();
        const auto binding_hash = dtl::hash(0, binding);
        const auto descriptor_hash = dtl::hash(0, images);
        const auto is_bound =
            std::find_if(bound.begin(), bound.end(), [=](const auto& each) {
                return each.binding == binding_hash;
            });
        const auto found_binding = is_bound != bound.end();
        crd_unlikely_if(!found_binding || is_bound->descriptor != descriptor_hash) {
            spdlog::info("updating image dynamic descriptor with binding: {}, images: {}", binding.index, images.size());
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
            vkUpdateDescriptorSets(context->device, 1, &update, 0, nullptr);
            if (found_binding) {
                is_bound->descriptor = descriptor_hash;
            } else {
                bound.push_back({ binding_hash, descriptor_hash });
            }
        }
        return *this;
    }

    crd_module DescriptorSet<1>& DescriptorSet<1>::bind(const DescriptorBinding& binding, VkDescriptorBufferInfo buffer) noexcept {
        crd_profile_scoped();
        return bind(binding, 0, buffer);
    }

    crd_module DescriptorSet<1>& DescriptorSet<1>::bind(const DescriptorBinding& binding, VkDescriptorImageInfo image) noexcept {
        crd_profile_scoped();
        return bind(binding, 0, image);
    }

#if defined(crd_enable_raytracing)
    crd_module DescriptorSet<1>& DescriptorSet<1>::bind(const DescriptorBinding& binding, const AccelerationStructure& tlas) noexcept {
        crd_profile_scoped();
        return bind(binding, 0, tlas);
    }
#endif

    crd_module DescriptorSet<1>& DescriptorSet<1>::bind(const DescriptorBinding& binding, const std::vector<VkDescriptorImageInfo>& images) noexcept {
        crd_profile_scoped();
        return bind(binding, 0, images);
    }

    crd_module void DescriptorSet<1>::destroy() noexcept {
        crd_profile_scoped();
        crd_vulkan_check(vkFreeDescriptorSets(context->device, context->descriptor_pool, 1, &handle));
        *this = {};
    }

    crd_module DescriptorSet<in_flight>& DescriptorSet<in_flight>::bind(const DescriptorBinding& binding, VkDescriptorBufferInfo buffer) noexcept {
        crd_profile_scoped();
        for (auto& each : handles) {
            each.bind(binding, buffer);
        }
        return *this;
    }

    crd_module DescriptorSet<in_flight>& DescriptorSet<in_flight>::bind(const DescriptorBinding& binding, VkDescriptorImageInfo image) noexcept {
        crd_profile_scoped();
        for (auto& each : handles) {
            each.bind(binding, image);
        }
        return *this;
    }

#if defined(crd_enable_raytracing)
    crd_module DescriptorSet<in_flight>& DescriptorSet<in_flight>::bind(const DescriptorBinding& binding, const AccelerationStructure& tlas) noexcept {
        crd_profile_scoped();
        for (auto& each : handles) {
            each.bind(binding, tlas);
        }
        return *this;
    }
#endif

    crd_module DescriptorSet<in_flight>& DescriptorSet<in_flight>::bind(const DescriptorBinding& binding, const std::vector<VkDescriptorImageInfo>& images) noexcept {
        crd_profile_scoped();
        for (auto& each : handles) {
            each.bind(binding, images);
        }
        return *this;
    }

    crd_module DescriptorSet<in_flight>& DescriptorSet<in_flight>::bind(const DescriptorBinding& binding, std::uint32_t offset, VkDescriptorBufferInfo buffer) noexcept {
        crd_profile_scoped();
        for (auto& each : handles) {
            each.bind(binding, offset, buffer);
        }
        return *this;
    }

    crd_module DescriptorSet<in_flight>& DescriptorSet<in_flight>::bind(const DescriptorBinding& binding, std::uint32_t offset, VkDescriptorImageInfo image) noexcept {
        crd_profile_scoped();
        for (auto& each : handles) {
            each.bind(binding, offset, image);
        }
        return *this;
    }

#if defined(crd_enable_raytracing)
    crd_module DescriptorSet<in_flight>& DescriptorSet<in_flight>::bind(const DescriptorBinding& binding, std::uint32_t offset, const AccelerationStructure& tlas) noexcept {
        crd_profile_scoped();
        for (auto& each : handles) {
            each.bind(binding, offset, tlas);
        }
        return *this;
    }
#endif

    crd_module DescriptorSet<in_flight>& DescriptorSet<in_flight>::bind(const DescriptorBinding& binding, std::uint32_t offset, const std::vector<VkDescriptorImageInfo>& images) noexcept {
        crd_profile_scoped();
        for (auto& each : handles) {
            each.bind(binding, offset, images);
        }
        return *this;
    }

    crd_nodiscard crd_module const DescriptorSet<1>& DescriptorSet<in_flight>::operator [](std::size_t index) const noexcept {
        crd_profile_scoped();
        return handles[index];
    }

    crd_nodiscard crd_module DescriptorSet<1>& DescriptorSet<in_flight>::operator [](std::size_t index) noexcept {
        crd_profile_scoped();
        return handles[index];
    }

    crd_module void DescriptorSet<in_flight>::destroy() noexcept {
        crd_profile_scoped();
        for (auto& each : handles) {
            each.destroy();
        }
        *this = {};
    }
} // namespace crd
