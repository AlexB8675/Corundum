#pragma once

#include <corundum/util/macros.hpp>

#include <vulkan/vulkan.h>

namespace crd::core {
    struct Context {
        VkInstance instance;
        VkPhysicalDevice gpu;
        VkDevice device;
#if crd_debug == 1
        VkDebugUtilsMessengerEXT validation;
#endif
    };

    crd_nodiscard Context make_context() noexcept;
} // namespace crd::core
