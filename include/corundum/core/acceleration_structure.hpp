#pragma once

#include <corundum/core/buffer.hpp>

#include <corundum/detail/forward.hpp>
#include <corundum/detail/macros.hpp>

#include <vulkan/vulkan.h>

namespace crd {
    struct AccelerationStructure {
        VkAccelerationStructureKHR handle;
        VkAccelerationStructureTypeKHR type;
        VkDeviceAddress address;
        StaticBuffer buffer;
    protected:
        explicit AccelerationStructure(VkAccelerationStructureTypeKHR) noexcept;
    };

    struct BottomLevelAS : AccelerationStructure {
        BottomLevelAS() noexcept;
    };

    struct TopLevelAS : AccelerationStructure {
        TopLevelAS() noexcept;

        StaticBuffer instances;
        StaticBuffer build;
    };
} // namespace crd