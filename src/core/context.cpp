#include <corundum/core/context.hpp>
#include <corundum/detail/logger.hpp>

#include <string_view>
#include <optional>
#include <utility>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <array>

#define load_instance_function(instance, fn) const auto fn = reinterpret_cast<PFN_##fn>(vkGetInstanceProcAddr(instance, #fn))

namespace crd {
    crd_nodiscard Context make_context() noexcept {
        detail::log("Vulkan", detail::severity_info, detail::type_general, "vulkan initialization started");
        Context context = {};
        { // Creates a VkInstance.
            detail::log("Vulkan", detail::severity_info, detail::type_general, "requesting vulkan version 1.2");
            VkApplicationInfo application_info;
            application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            application_info.pNext = nullptr;
            application_info.pApplicationName = "Corundum Engine";
            application_info.applicationVersion = VK_API_VERSION_1_2;
            application_info.pEngineName = "Corundum Engine";
            application_info.engineVersion = VK_API_VERSION_1_2;
            application_info.apiVersion = VK_API_VERSION_1_2;

            detail::log("Vulkan", detail::severity_verbose, detail::type_general, "enumerating extensions:");
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
                crd_likely_if(std::string_view(name).find("debug") == std::string::npos) {
                    detail::log("Vulkan", detail::severity_verbose, detail::type_general, "  - %s", name);
                    extension_names.emplace_back(name);
                }
            }

            VkInstanceCreateInfo instance_info;
            instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            instance_info.pNext = nullptr;
            instance_info.flags = {};
            instance_info.pApplicationInfo = &application_info;
#if defined(crd_debug)
            detail::log("Vulkan", detail::severity_info, detail::type_general, "debug mode active, requesting validation layers");
            const char* validation_layer = "VK_LAYER_KHRONOS_validation";
            instance_info.enabledLayerCount = 1;
            instance_info.ppEnabledLayerNames = &validation_layer;
#else
            instance_info.enabledLayerCount = 0;
            instance_info.ppEnabledLayerNames = nullptr;
#endif
            instance_info.enabledExtensionCount = extension_names.size();
            instance_info.ppEnabledExtensionNames = extension_names.data();

            crd_vulkan_check(vkCreateInstance(&instance_info, nullptr, &context.instance));
            detail::log("Vulkan", detail::severity_info, detail::type_general, "extensions enabled successfully");
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

                detail::log("Vulkan", severity_string, type_string, data->pMessage);
                const auto fatal_bits = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
                crd_unlikely_if(severity & fatal_bits) {
                    crd_force_assert("Fatal Vulkan Error has occurred");
                }
                return 0;
            };
            validation_info.pUserData = nullptr;

            detail::log("Vulkan", detail::severity_info, detail::type_general, "loading validation function: vkCreateDebugUtilsMessengerEXT");
            load_instance_function(context.instance, vkCreateDebugUtilsMessengerEXT);
            crd_assert(vkCreateDebugUtilsMessengerEXT, "failed loading validation function");
            crd_vulkan_check(vkCreateDebugUtilsMessengerEXT(context.instance, &validation_info, nullptr, &context.validation));
            detail::log("Vulkan", detail::severity_info, detail::type_general, "validation layers enabled successfully");
        }
#endif
        { // Picks a VkPhysicalDevice.
            std::uint32_t device_count;
            vkEnumeratePhysicalDevices(context.instance, &device_count, nullptr);
            std::vector<VkPhysicalDevice> devices(device_count);
            vkEnumeratePhysicalDevices(context.instance, &device_count, devices.data());

            detail::log("Vulkan", detail::severity_info, detail::type_general, "enumerating physical devices:");
            for (const auto& gpu : devices) {
                VkPhysicalDeviceProperties properties;
                vkGetPhysicalDeviceProperties(gpu, &properties);
                detail::log("Vulkan", detail::severity_info, detail::type_general, "  - found device: %s", properties.deviceName);
                const auto device_criteria =
                    VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU |
                    VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU   |
                    VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU;
                crd_likely_if(properties.deviceType & device_criteria) {
                    detail::log("Vulkan", detail::severity_info, detail::type_general, "  - chosen device: %s", properties.deviceName);
                    const auto driver_major = VK_VERSION_MAJOR(properties.driverVersion);
                    const auto driver_minor = VK_VERSION_MINOR(properties.driverVersion);
                    const auto driver_patch = VK_VERSION_PATCH(properties.driverVersion);
                    const auto vulkan_major = VK_VERSION_MAJOR(properties.apiVersion);
                    const auto vulkan_minor = VK_VERSION_MINOR(properties.apiVersion);
                    const auto vulkan_patch = VK_VERSION_PATCH(properties.apiVersion);
                    detail::log("Vulkan", detail::severity_info, detail::type_general, "  - driver version: %d.%d.%d", driver_major, driver_minor, driver_patch);
                    detail::log("Vulkan", detail::severity_info, detail::type_general, "  - vulkan version: %d.%d.%d", vulkan_major, vulkan_minor, vulkan_patch);
                    context.gpu_properties = properties;
                    context.gpu = gpu;
                    break;
                }
            }
            crd_assert(context.gpu, "no suitable GPU found in the system");
        }
        { // Chooses queue families and creates a VkDevice.
            detail::log("Vulkan", detail::severity_info, detail::type_general, "enumerating queue families");
            std::uint32_t families_count;
            vkGetPhysicalDeviceQueueFamilyProperties(context.gpu, &families_count, nullptr);
            std::vector<VkQueueFamilyProperties> queue_families(families_count);
            vkGetPhysicalDeviceQueueFamilyProperties(context.gpu, &families_count, queue_families.data());

            std::vector<std::uint32_t> queue_sizes(families_count);
            std::vector<std::vector<float>> queue_priorities(families_count);
            const auto fetch_family = [&](VkQueueFlags required, VkQueueFlags ignore, float priority) noexcept -> std::optional<QueueFamily> {
                for (std::uint32_t family = 0; family < families_count; family++) {
                    crd_unlikely_if((queue_families[family].queueFlags & ignore) != 0) {
                        continue;
                    }

                    const auto remaining = queue_families[family].queueCount - queue_sizes[family];
                    crd_likely_if(remaining > 0 && (queue_families[family].queueFlags & required) == required) {
                        queue_priorities[family].emplace_back(priority);
                        return QueueFamily{
                            family, queue_sizes[family]++
                        };
                    }
                }
                return std::nullopt;
            };

            QueueFamilies families;
            if (auto queue = fetch_family(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT, 0, 0.5f)) {
                families.graphics = *queue;
            } else {
                crd_force_assert("no suitable Graphics queue found");
            }

            if (auto queue = fetch_family(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT, 0, 1.0f)) {
                // Prefer another graphics queue since we can do async graphics that way.
                families.compute = *queue;
            } else if (auto fallback = fetch_family(VK_QUEUE_COMPUTE_BIT, VK_QUEUE_GRAPHICS_BIT, 1.0f)) {
                // Fallback to a dedicated compute queue which doesn't support graphics.
                families.compute = *fallback;
            } else {
                // Finally, fallback to graphics queue
                families.compute = families.graphics;
            }


            if (auto queue = fetch_family(VK_QUEUE_TRANSFER_BIT, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT, 0.5f)) {
                // Find a queue which only supports transfer.
                families.transfer = *queue;
            } else if (auto fallback = fetch_family(VK_QUEUE_COMPUTE_BIT, VK_QUEUE_GRAPHICS_BIT, 0.5f)) {
                // Fallback to a dedicated compute queue.
                families.transfer = *fallback;
            } else {
                // Finally, fallback to same queue as compute.
                families.transfer = families.compute;
            }

            context.families = families;
            detail::log("Vulkan", detail::severity_info, detail::type_general, "chosen families: %d, %d, %d", families.graphics.family, families.transfer.family, families.compute.family);
            std::vector<VkDeviceQueueCreateInfo> queue_infos;
            for (std::uint32_t family = 0; family < families_count; family++) {
                crd_unlikely_if(queue_sizes[family] == 0) {
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

            detail::log("Vulkan", detail::severity_info, detail::type_general, "fetching device features");
            VkPhysicalDeviceFeatures features;
            vkGetPhysicalDeviceFeatures(context.gpu, &features);
            detail::log("Vulkan", detail::severity_info, detail::type_general, "enumerating device extensions");
            std::uint32_t extension_count;
            vkEnumerateDeviceExtensionProperties(context.gpu, nullptr, &extension_count, nullptr);
            std::vector<VkExtensionProperties> extensions_props(extension_count);
            vkEnumerateDeviceExtensionProperties(context.gpu, nullptr, &extension_count, extensions_props.data());
            constexpr std::array extension_names = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
            };

            detail::log("Vulkan", detail::severity_info, detail::type_general, "requesting ownership of device");
            VkPhysicalDeviceDescriptorIndexingFeatures descriptor_indexing = {};
            descriptor_indexing.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
            descriptor_indexing.shaderSampledImageArrayNonUniformIndexing = true;
            descriptor_indexing.shaderUniformBufferArrayNonUniformIndexing = true;
            descriptor_indexing.shaderStorageBufferArrayNonUniformIndexing = true;
            descriptor_indexing.descriptorBindingVariableDescriptorCount = true;
            descriptor_indexing.descriptorBindingPartiallyBound = true;
            descriptor_indexing.runtimeDescriptorArray = true;
            VkDeviceCreateInfo device_info;
            device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            device_info.pNext = &descriptor_indexing;
            device_info.flags = {};
            device_info.pQueueCreateInfos = queue_infos.data();
            device_info.queueCreateInfoCount = queue_infos.size();
            device_info.enabledLayerCount = 0;
            device_info.ppEnabledLayerNames = nullptr;
            device_info.enabledExtensionCount = extension_names.size();
            device_info.ppEnabledExtensionNames = extension_names.data();
            device_info.pEnabledFeatures = &features;
            crd_vulkan_check(vkCreateDevice(context.gpu, &device_info, nullptr, &context.device));
            detail::log("Vulkan", detail::severity_info, detail::type_general, "ownership acquired successfully, locked");

            context.graphics = make_queue(context, families.graphics);
            context.transfer = make_queue(context, families.transfer);
            context.compute = make_queue(context, families.compute);
            detail::log("Vulkan", detail::severity_info, detail::type_general, "device queues initialized");
        }
        { // Creates the Task Scheduler.
            detail::log("Vulkan", detail::severity_info, detail::type_general, "initializing task scheduler");
            (context.scheduler = new ftl::TaskScheduler())->Init({
                .Behavior = ftl::EmptyQueueBehavior::Sleep
            });
        }
        { // Creates a Descriptor Pool.
            const auto max_samplers = max_bound_samplers(context);
            const auto descriptor_sizes = std::to_array<VkDescriptorPoolSize>({
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         2048         },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         2048         },
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, max_samplers },
            });

            VkDescriptorPoolCreateInfo descriptor_pool_info;
            descriptor_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            descriptor_pool_info.pNext = nullptr;
            descriptor_pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            descriptor_pool_info.maxSets = 4096 + max_samplers;
            descriptor_pool_info.poolSizeCount = descriptor_sizes.size();
            descriptor_pool_info.pPoolSizes = descriptor_sizes.data();
            crd_vulkan_check(vkCreateDescriptorPool(context.device, &descriptor_pool_info, nullptr, &context.descriptor_pool));
        }
        { // Creates a VkSampler (default).
            VkSamplerCreateInfo sampler_info;
            sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            sampler_info.pNext = nullptr;
            sampler_info.flags = {};
            sampler_info.magFilter = VK_FILTER_LINEAR;
            sampler_info.minFilter = VK_FILTER_LINEAR;
            sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            sampler_info.mipLodBias = 0;
            sampler_info.anisotropyEnable = true;
            sampler_info.maxAnisotropy = 16;
            sampler_info.compareEnable = false;
            sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
            sampler_info.minLod = 0;
            sampler_info.maxLod = 8;
            sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
            sampler_info.unnormalizedCoordinates = false;
            crd_vulkan_check(vkCreateSampler(context.device, &sampler_info, nullptr, &context.default_sampler));
            sampler_info.magFilter = VK_FILTER_NEAREST;
            sampler_info.minFilter = VK_FILTER_NEAREST;
            sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
            crd_vulkan_check(vkCreateSampler(context.device, &sampler_info, nullptr, &context.shadow_sampler));
        }
        { // Creates a VmaAllocator.
            detail::log("Vulkan", detail::severity_info, detail::type_general, "creating allocator");
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
            detail::log("Vulkan", detail::severity_info, detail::type_general, "allocator created successfully");
        }
        detail::log("Vulkan", detail::severity_info, detail::type_general, "vulkan initialization completed");
        return context;
    }

    crd_module void destroy_context(Context& context) noexcept {
        detail::log("Vulkan", detail::severity_info, detail::type_general, "terminating core context");
        delete context.scheduler;
        destroy_queue(context, context.graphics);
        destroy_queue(context, context.transfer);
        destroy_queue(context, context.compute);
        vkDestroyDescriptorPool(context.device, context.descriptor_pool, nullptr);
        vkDestroySampler(context.device, context.shadow_sampler, nullptr);
        vkDestroySampler(context.device, context.default_sampler, nullptr);
        vmaDestroyAllocator(context.allocator);
        vkDestroyDevice(context.device, nullptr);
#if defined(crd_debug)
        load_instance_function(context.instance, vkDestroyDebugUtilsMessengerEXT);
        vkDestroyDebugUtilsMessengerEXT(context.instance, context.validation, nullptr);
#endif
        vkDestroyInstance(context.instance, nullptr);
        context = {};
        detail::log("Vulkan", detail::severity_info, detail::type_general, "termination complete");
    }

    crd_nodiscard crd_module std::uint32_t max_bound_samplers(const Context& context) noexcept {
        return std::min(context.gpu_properties.limits.maxPerStageDescriptorSampledImages, 32768u);
    }
} //namespace crd
