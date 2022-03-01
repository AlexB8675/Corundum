#pragma once

#include <corundum/detail/forward.hpp>
#include <corundum/detail/macros.hpp>

#include <vulkan/vulkan.h>

namespace crd {
    struct AccelerationStructure {
        VkAccelerationStructureKHR handle;
        VkAccelerationStructureTypeKHR type;
    };
} // namespace crd