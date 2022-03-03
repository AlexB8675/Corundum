#pragma once

#include <corundum/detail/macros.hpp>

#include <vulkan/vulkan.h>

namespace crd {
#if defined(crd_enable_raytracing)
    crd_module inline PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
    crd_module inline PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
    crd_module inline PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
    crd_module inline PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
    crd_module inline PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
    crd_module inline PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
    crd_module inline PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
#endif
} // namespace crd
