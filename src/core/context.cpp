#include <corundum/core/context.hpp>

namespace crd::core {
    crd_nodiscard static inline VkInstance make_instance() noexcept {
        VkApplicationInfo application_info;
        application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        application_info.pNext = nullptr;
        application_info.pApplicationName = "Corundum Engine";
        application_info.applicationVersion = VK_VERSION_1_2;
        application_info.pEngineName = "Corundum Engine";
        application_info.engineVersion = VK_VERSION_1_2;
        application_info.apiVersion = VK_VERSION_1_2;

        VkInstanceCreateInfo instance_info;
        instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_info.pNext = nullptr;
        instance_info.flags = {};
        instance_info.pApplicationInfo = &application_info;
        instance_info.enabledLayerCount;
        instance_info.ppEnabledLayerNames;
        instance_info.enabledExtensionCount;
        instance_info.ppEnabledExtensionNames;
    }

    crd_nodiscard Context make_context() noexcept {
        Context context;

        return context;
    }
} //namespace crd::core