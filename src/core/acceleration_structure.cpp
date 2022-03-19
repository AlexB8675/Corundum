#include <corundum/core/acceleration_structure.hpp>

namespace crd {
    AccelerationStructure::AccelerationStructure(VkAccelerationStructureTypeKHR type) noexcept
        : handle(),
          type(type),
          buffer() {}

    BottomLevelAS::BottomLevelAS() noexcept
        : AccelerationStructure(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR) {}

    TopLevelAS::TopLevelAS() noexcept
        : AccelerationStructure(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR),
          instances(),
          build() {}
} // namespace crd
