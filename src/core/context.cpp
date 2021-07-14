#include <corundum/core/context.hpp>
#include <corundum/util/logger.hpp>

#include <string_view>
#include <optional>
#include <vector>
#include <string>
#include <array>

#define load_instance_function(instance, fn) const auto fn = reinterpret_cast<PFN_##fn>(vkGetInstanceProcAddr(instance, #fn))

namespace crd::core {
    crd_nodiscard Context make_context() noexcept {
        util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral, "Vulkan initialization started");
        Context context = {};
        { // Creates a VkInstance.
            util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral, "Requesting Vulkan Version 1.2");
            VkApplicationInfo application_info;
            application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            application_info.pNext = nullptr;
            application_info.pApplicationName = "Corundum Engine";
            application_info.applicationVersion = VK_VERSION_1_2;
            application_info.pEngineName = "Corundum Engine";
            application_info.engineVersion = VK_VERSION_1_2;
            application_info.apiVersion = VK_VERSION_1_2;

            util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral, "Enumerating extensions:");
            std::uint32_t extension_count;
            vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
            std::vector<VkExtensionProperties> extensions_props(extension_count);
            vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions_props.data());
            std::vector<const char*> extension_names;
            extension_names.reserve(extension_count);
#if defined(crd_debug)
            extension_names.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
            for (const auto& [name, _] : extensions_props) {
                if (std::string_view(name).find("debug") == std::string::npos) {
                    util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral, "  - %s", name);
                    extension_names.emplace_back(name);
                }
            }

            VkInstanceCreateInfo instance_info;
            instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            instance_info.pNext = nullptr;
            instance_info.flags = {};
            instance_info.pApplicationInfo = &application_info;
#if defined(crd_debug)
            util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral, "Debug mode active, requesting validation layers");
            const char* validation_layer = "VK_LAYER_KHRONOS_validation";
            instance_info.enabledLayerCount = 1;
            instance_info.ppEnabledLayerNames = &validation_layer;
#endif
            instance_info.enabledExtensionCount = extension_names.size();
            instance_info.ppEnabledExtensionNames = extension_names.data();

            crd_vulkan_check(vkCreateInstance(&instance_info, nullptr, &context.instance));
            util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral, "Extensions enabled successfully");
        }
#if defined(crd_debug)
        { // Installs validation layers only if debug mode is active.
            VkDebugUtilsMessengerCreateInfoEXT validation_info;
            validation_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            validation_info.pNext = nullptr;
            validation_info.flags = {};
            validation_info.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            validation_info.messageType =
                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT    |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            validation_info.pfnUserCallback = +[](
                VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
                VkDebugUtilsMessageTypeFlagsEXT             type,
                const VkDebugUtilsMessengerCallbackDataEXT* data,
                void*) -> VkBool32 {
                const char* severity_string = nullptr;
                switch (severity) {
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: severity_string = "Verbose"; break;
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:    severity_string = "Info";   break;
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: severity_string = "Warning"; break;
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:   severity_string = "Fatal";   break;

                }
                const char* type_string = nullptr;
                switch (type) {
                    case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:     type_string = "General"; break;
                    case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:  type_string = "Validation"; break;
                    case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT: type_string = "Performance"; break;
                }

                util::log("Vulkan", severity_string, type_string, data->pMessage);
                const auto fatal_bits = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
                crd_assert(severity & fatal_bits, "Fatal Vulkan Error has occurred");
                return 0;
            };
            validation_info.pUserData = nullptr;

            util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral, "Loading validation function: vkCreateDebugUtilsMessengerEXT");
            load_instance_function(context.instance, vkCreateDebugUtilsMessengerEXT);
            crd_assert(vkCreateDebugUtilsMessengerEXT, "Failed loading validation function");
            crd_vulkan_check(vkCreateDebugUtilsMessengerEXT(context.instance, &validation_info, nullptr, &context.validation));
            util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral, "Validation layers enabled successfully");
        }
#endif
        { // Picks a VkPhysicalDevice.
            std::uint32_t device_count;
            vkEnumeratePhysicalDevices(context.instance, &device_count, nullptr);
            std::vector<VkPhysicalDevice> devices(device_count);
            vkEnumeratePhysicalDevices(context.instance, &device_count, devices.data());

            util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral, "Enumerating physical devices:");
            for (const auto& gpu : devices) {
                VkPhysicalDeviceProperties properties;
                vkGetPhysicalDeviceProperties(gpu, &properties);
                util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral,"  - Found device: %s", properties.deviceName);
                const auto device_criteria =
                    VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU |
                    VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU   |
                    VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU;
                if (properties.deviceType & device_criteria) {
                    util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral,"  - Chosen device: %s", properties.deviceName);
                    context.gpu = gpu;
                }
            }
            crd_assert(context.gpu, "No suitable GPU found in the system");
        }
        { // Chooses queue families and creates a VkDevice.
            util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral, "Enumerating Queue families");
            std::uint32_t families_count;
            vkGetPhysicalDeviceQueueFamilyProperties(context.gpu, &families_count, nullptr);
            std::vector<VkQueueFamilyProperties> queue_families(families_count);
            vkGetPhysicalDeviceQueueFamilyProperties(context.gpu, &families_count, queue_families.data());

            std::vector<std::uint32_t> queue_sizes(families_count);
            std::vector<std::vector<float>> queue_priorities(families_count);
            const auto fetch_family = [&](VkQueueFlags required, VkQueueFlags ignore, float priority) -> std::optional<QueueFamily> {
                for (std::uint32_t family = 0; family < families_count; family++) {
                    if ((queue_families[family].queueFlags & ignore) != 0) {
                        continue;
                    }

                    const auto remaining = queue_families[family].queueCount - queue_sizes[family];
                    if (remaining > 0 && (queue_families[family].queueFlags & required) == required) {
                        queue_priorities[family].emplace_back(priority);
                        return QueueFamily{
                            family, queue_sizes[family]++
                        };
                    }
                }
                return std::nullopt;
            };

            QueueFamilies families;
            if (const auto&& queue = fetch_family(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT, 0, 0.5f)) {
                families.graphics = queue.value();
            } else {
                crd_force_assert("No suitable Graphics queue found");
            }

            if (const auto&& queue = fetch_family(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT, 0, 1.0f)) {
                // Prefer another graphics queue since we can do async graphics that way.
                families.compute = queue.value();
            } else if (const auto&& fallback = fetch_family(VK_QUEUE_COMPUTE_BIT, VK_QUEUE_GRAPHICS_BIT, 1.0f)) {
                // Fallback to a dedicated compute queue which doesn't support graphics.
                families.compute = fallback.value();
            } else {
                // Finally, fallback to graphics queue
                families.compute = families.graphics;
            }


            if (const auto&& queue = fetch_family(VK_QUEUE_TRANSFER_BIT, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT, 0.5f)) {
                // Find a queue which only supports transfer.
                families.transfer = queue.value();
            } else if (const auto&& fallback = fetch_family(VK_QUEUE_COMPUTE_BIT, VK_QUEUE_GRAPHICS_BIT, 0.5f)) {
                // Fallback to a dedicated compute queue.
                families.transfer = fallback.value();
            } else {
                // Finally, fallback to same queue as compute.
                families.transfer = families.compute;
            }

            context.families = families;
            util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral, "Chosen families: %d, %d, %d", families.graphics.family, families.transfer.family, families.compute.family);
            std::vector<VkDeviceQueueCreateInfo> queue_infos;
            for (std::uint32_t family = 0; family < families_count; family++) {
                if (queue_sizes[family] == 0) {
                    continue;
                }

                VkDeviceQueueCreateInfo queue_info;
                queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queue_info.pNext = nullptr;
                queue_info.flags = {};
                queue_info.queueFamilyIndex = family;
                queue_info.queueCount = queue_sizes[family];
                queue_info.pQueuePriorities = queue_priorities[family].data();
                queue_infos.emplace_back(queue_info);
            }

            util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral, "Fetching device features");
            VkPhysicalDeviceFeatures features;
            vkGetPhysicalDeviceFeatures(context.gpu, &features);

            util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral, "Enumerating device extensions");
            std::uint32_t extension_count;
            vkEnumerateDeviceExtensionProperties(context.gpu, nullptr, &extension_count, nullptr);
            std::vector<VkExtensionProperties> extensions_props(extension_count);
            vkEnumerateDeviceExtensionProperties(context.gpu, nullptr, &extension_count, extensions_props.data());
            for (const auto& [name, _] : extensions_props) {
                util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral, "  - %s", name);
            }

            util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral, "Requesting ownership of device");
            constexpr std::array extension_names = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME
            };
            VkDeviceCreateInfo device_info;
            device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            device_info.pNext = nullptr;
            device_info.flags = {};
            device_info.pQueueCreateInfos = queue_infos.data();
            device_info.queueCreateInfoCount = queue_infos.size();
            device_info.enabledLayerCount = 0;
            device_info.ppEnabledLayerNames = nullptr;
            device_info.enabledExtensionCount = extension_names.size();
            device_info.ppEnabledExtensionNames = extension_names.data();
            device_info.pEnabledFeatures = &features;
            crd_vulkan_check(vkCreateDevice(context.gpu, &device_info, nullptr, &context.device));
            util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral, "Ownership acquired successfully, locked");

            context.graphics = make_queue(context, families.graphics);
            context.transfer = make_queue(context, families.transfer);
            context.compute = make_queue(context, families.compute);
            util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral, "Device queues initialized");
        }
        { // Creates a VmaAllocator.
            util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral, "Creating allocator");
            VmaAllocatorCreateInfo allocator_info;
            allocator_info.flags = {};
            allocator_info.physicalDevice = context.gpu;
            allocator_info.device = context.device;
            allocator_info.preferredLargeHeapBlockSize = 0;
            allocator_info.pAllocationCallbacks = nullptr;
            allocator_info.pDeviceMemoryCallbacks = nullptr;
            allocator_info.frameInUseCount = 1;
            allocator_info.pHeapSizeLimit = nullptr;
            allocator_info.pVulkanFunctions = nullptr;
            allocator_info.pRecordSettings = nullptr;
            allocator_info.instance = context.instance;
            allocator_info.vulkanApiVersion = VK_API_VERSION_1_2;
            allocator_info.pTypeExternalMemoryHandleTypes = nullptr;
            crd_vulkan_check(vmaCreateAllocator(&allocator_info, &context.allocator));
            util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral, "Allocator created successfully");
        }
        util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral, "Vulkan initialization completed");
        return context;
    }
} //namespace crd::core
