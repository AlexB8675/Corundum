#include <corundum/core/context.hpp>
#include <corundum/util/logger.hpp>

#include <string_view>
#include <optional>
#include <utility>
#include <cstdlib>
#include <cstring>
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
            application_info.applicationVersion = VK_API_VERSION_1_2;
            application_info.pEngineName = "Corundum Engine";
            application_info.engineVersion = VK_API_VERSION_1_2;
            application_info.apiVersion = VK_API_VERSION_1_2;

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
                crd_likely_if(std::string_view(name).find("debug") == std::string::npos) {
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
#else
            instance_info.enabledLayerCount = 0;
            instance_info.ppEnabledLayerNames = nullptr;
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
                crd_unlikely_if(severity & fatal_bits) {
                    crd_force_assert("Fatal Vulkan Error has occurred");
                }
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
                util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral, "  - Found device: %s", properties.deviceName);
                const auto device_criteria =
                    VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU |
                    VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU   |
                    VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU;
                crd_likely_if(properties.deviceType & device_criteria) {
                    util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral, "  - Chosen device: %s", properties.deviceName);
                    const auto driver_major = VK_VERSION_MAJOR(properties.driverVersion);
                    const auto driver_minor = VK_VERSION_MINOR(properties.driverVersion);
                    const auto driver_patch = VK_VERSION_PATCH(properties.driverVersion);
                    const auto vulkan_major = VK_VERSION_MAJOR(properties.apiVersion);
                    const auto vulkan_minor = VK_VERSION_MINOR(properties.apiVersion);
                    const auto vulkan_patch = VK_VERSION_PATCH(properties.apiVersion);
                    util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral, "  - Driver Version: %d.%d.%d", driver_major, driver_minor, driver_patch);
                    util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral, "  - Vulkan Version: %d.%d.%d", vulkan_major, vulkan_minor, vulkan_patch);
                    context.gpu_properties = properties;
                    context.gpu = gpu;
                    break;
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
            util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral, "Ownership acquired successfully, locked");

            context.graphics = make_queue(context, families.graphics);
            context.transfer = make_queue(context, families.transfer);
            context.compute = make_queue(context, families.compute);
            util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral, "Device queues initialized");
        }
        { // Creates the Task Scheduler.
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
        }
        { // Creates a VmaAllocator.
            VkAllocationCallbacks allocation_callbacks;
            allocation_callbacks.pUserData = nullptr;
            allocation_callbacks.pfnAllocation =
                [](void*, std::size_t size, std::size_t align, VkSystemAllocationScope scope) noexcept {
                    void* memory = std::malloc(size);
                    util::log("Vulkan", util::Severity::eInfo, util::Type::eValidation,
                              "CPU Allocation Requested, Size: %zu bytes, Alignment: %zu bytes, Scope: %d, Address: %p",
                              size, align, scope, memory);
                    return memory;
                };
            allocation_callbacks.pfnReallocation =
                [](void*, void* old, std::size_t size, std::size_t align, VkSystemAllocationScope scope) noexcept {
                    void* memory = std::realloc(old, size);
                    util::log("Vulkan", util::Severity::eInfo, util::Type::eValidation,
                              "CPU Reallocation Requested, Size: %zu bytes, Alignment: %zu bytes, Scope: %d, Address: %p",
                              size, align, scope, memory);
                    return memory;
                };
            allocation_callbacks.pfnFree =
                [](void*, void* memory) noexcept {
                    if (memory) {
                        util::log("Vulkan", util::Severity::eInfo, util::Type::eValidation, "CPU Free Requested, Address: %p", memory);
                        std::free(memory);
                    }
                };
            allocation_callbacks.pfnInternalAllocation =
                [](void*, std::size_t size, VkInternalAllocationType type, VkSystemAllocationScope scope) noexcept {
                    util::log("Vulkan", util::Severity::eInfo, util::Type::eValidation,
                              "Internal Allocation Requested, Size: %zu bytes, Type: %d, Scope: %d",
                              size, type, scope);
                };
            allocation_callbacks.pfnInternalFree =
                [](void*, std::size_t size, VkInternalAllocationType type, VkSystemAllocationScope scope) noexcept {
                    util::log("Vulkan", util::Severity::eInfo, util::Type::eValidation,
                              "Internal Free Requested, Size: %zu bytes, Type: %d, Scope: %d",
                              size, type, scope);
                };
            VmaDeviceMemoryCallbacks device_memory_callbacks;
            device_memory_callbacks.pfnAllocate =
                [](VmaAllocator allocator, std::uint32_t type, VkDeviceMemory memory, VkDeviceSize size, void*) noexcept {
                    util::log("Vulkan", util::Severity::eInfo, util::Type::eValidation,
                              "GPU Allocation Requested, Allocator: %p, Size: %zu bytes, Type: %d, Address: %p",
                              allocator, size, type, memory);
                };
            device_memory_callbacks.pfnFree =
                [](VmaAllocator allocator, std::uint32_t type, VkDeviceMemory memory, VkDeviceSize size, void*) noexcept {
                    util::log("Vulkan", util::Severity::eInfo, util::Type::eValidation,
                              "GPU Free Requested, Allocator: %p, Size: %zu bytes, Type: %d, Address: %p",
                              allocator, size, type, memory);
                };
            device_memory_callbacks.pUserData = nullptr;
            util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral, "Creating allocator");
            VmaAllocatorCreateInfo allocator_info;
            allocator_info.flags = {};
            allocator_info.physicalDevice = context.gpu;
            allocator_info.device = context.device;
            allocator_info.preferredLargeHeapBlockSize = 0;
            allocator_info.pAllocationCallbacks = &allocation_callbacks;
            allocator_info.pDeviceMemoryCallbacks = &device_memory_callbacks;
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

    crd_module void destroy_context(Context& context) noexcept {
        util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral, "Terminating Core Context");
        delete context.scheduler;
        destroy_queue(context, context.graphics);
        destroy_queue(context, context.transfer);
        destroy_queue(context, context.compute);
        vkDestroyDescriptorPool(context.device, context.descriptor_pool, nullptr);
        vkDestroySampler(context.device, context.default_sampler, nullptr);
        vmaDestroyAllocator(context.allocator);
        vkDestroyDevice(context.device, nullptr);
#if defined(crd_debug)
        load_instance_function(context.instance, vkDestroyDebugUtilsMessengerEXT);
        vkDestroyDebugUtilsMessengerEXT(context.instance, context.validation, nullptr);
#endif
        vkDestroyInstance(context.instance, nullptr);
        context = {};
        util::log("Vulkan", util::Severity::eInfo, util::Type::eGeneral, "Core Context terminated successfully");
    }

    crd_nodiscard crd_module std::uint32_t max_bound_samplers(const Context& context) noexcept {
        return std::min(context.gpu_properties.limits.maxPerStageDescriptorSampledImages, 32768u);
    }
} //namespace crd::core
