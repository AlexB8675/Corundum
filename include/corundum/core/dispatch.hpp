#pragma once

#include <corundum/detail/macros.hpp>

#include <vulkan/vulkan.h>

namespace crd {
#if defined(crd_enable_raytracing)
    crd_module inline PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
    crd_module inline PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
    crd_module inline PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
#endif
} // namespace crd
