#include <corundum/core/dispatch.hpp>
#include <corundum/core/context.hpp>

#include <corundum/detail/logger.hpp>

#include <GLFW/glfw3.h>

#include <string_view>
#include <optional>
#include <utility>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <array>
#include <span>

namespace crd {
    crd_nodiscard static inline bool has_extension(std::span<VkExtensionProperties> extensions, const char* extension) noexcept {
        for (const auto& [name, _] : extensions) {
            if (std::strcmp(name, extension) == 0) {
                return true;
            }
        }
        return false;
    }

    template <typename T, typename U>
    static inline void append_to_chain(T& object, U& next) noexcept {
        if (!object.pNext) {
            object.pNext = &next;
            return;
        }
        next.pNext = const_cast<void*>(object.pNext);
        object.pNext = &next;
    }

    static inline void initialize_dynamic_dispatcher(const Context& context) noexcept {
#if defined(crd_enable_raytracing)
        vkGetAccelerationStructureBuildSizesKHR = crd_load_device_function(context.device, vkGetAccelerationStructureBuildSizesKHR);
        vkCmdBuildAccelerationStructuresKHR = crd_load_device_function(context.device, vkCmdBuildAccelerationStructuresKHR);
        vkCreateAccelerationStructureKHR = crd_load_device_function(context.device, vkCreateAccelerationStructureKHR);
        vkGetAccelerationStructureDeviceAddressKHR = crd_load_device_function(context.device, vkGetAccelerationStructureDeviceAddressKHR);
        vkCreateRayTracingPipelinesKHR = crd_load_device_function(context.device, vkCreateRayTracingPipelinesKHR);
        vkGetRayTracingShaderGroupHandlesKHR = crd_load_device_function(context.device, vkGetRayTracingShaderGroupHandlesKHR);
        vkCmdTraceRaysKHR = crd_load_device_function(context.device, vkCmdTraceRaysKHR);
        vkDestroyAccelerationStructureKHR = crd_load_device_function(context.device, vkDestroyAccelerationStructureKHR);
#endif
    }

    crd_nodiscard Context make_context() noexcept {
        dtl::log("Vulkan", dtl::severity_info, dtl::type_general, "vulkan initialization started");
        Context context = {};
        { // Creates a VkInstance.
            dtl::log("Vulkan", dtl::severity_info, dtl::type_general, "requesting vulkan version 1.2");
            VkApplicationInfo application_info;
            application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            application_info.pNext = nullptr;
            application_info.pApplicationName = "Corundum Engine";
            application_info.applicationVersion = VK_API_VERSION_1_2;
            application_info.pEngineName = "Corundum Engine";
            application_info.engineVersion = VK_API_VERSION_1_2;
            application_info.apiVersion = VK_API_VERSION_1_2;

            dtl::log("Vulkan", dtl::severity_verbose, dtl::type_general, "enumerating extensions:");
            std::uint32_t extension_count;
            vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
            std::vector<VkExtensionProperties> extensions_props(extension_count);
            vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions_props.data());
            std::uint32_t surface_extension_count = 0;
            const char** surface_extensions = glfwGetRequiredInstanceExtensions(&surface_extension_count);
            std::vector<const char*> extension_names;
            extension_names.reserve(extension_count);
#if defined(crd_debug)
            extension_names.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
            extension_names.insert(extension_names.end(), surface_extensions, surface_extensions + surface_extension_count);
            VkInstanceCreateInfo instance_info;
            instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            instance_info.pNext = nullptr;
            instance_info.flags = {};
            instance_info.pApplicationInfo = &application_info;
#if defined(crd_debug)
    #if defined(crd_extra_validation)
            constexpr auto enabled_validation_features = std::to_array({
                VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT
            });
            VkValidationFeaturesEXT extra_validation;
            extra_validation.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
            extra_validation.pNext = nullptr;
            extra_validation.enabledValidationFeatureCount = enabled_validation_features.size();
            extra_validation.pEnabledValidationFeatures = enabled_validation_features.data();
            extra_validation.disabledValidationFeatureCount = 0;
            extra_validation.pDisabledValidationFeatures = nullptr;
            instance_info.pNext = &extra_validation;
    #endif
            dtl::log("Vulkan", dtl::severity_info, dtl::type_general, "debug mode active, requesting validation layers");
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
            dtl::log("Vulkan", dtl::severity_info, dtl::type_general, "extensions enabled successfully");
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

                dtl::log("Vulkan", severity_string, type_string, data->pMessage);
                const auto fatal_bits = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
                crd_unlikely_if(severity & fatal_bits) {
                    crd_force_assert("Fatal Vulkan error has occurred");
                }
                return 0;
            };
            validation_info.pUserData = nullptr;

            dtl::log("Vulkan", dtl::severity_info, dtl::type_general, "loading validation function: vkCreateDebugUtilsMessengerEXT");
            const auto vkCreateDebugUtilsMessengerEXT = crd_load_instance_function(context.instance, vkCreateDebugUtilsMessengerEXT);
            crd_assert(vkCreateDebugUtilsMessengerEXT, "failed loading validation function");
            crd_vulkan_check(vkCreateDebugUtilsMessengerEXT(context.instance, &validation_info, nullptr, &context.validation));
            dtl::log("Vulkan", dtl::severity_info, dtl::type_general, "validation layers enabled successfully");
        }
#endif
        { // Picks a VkPhysicalDevice.
            std::uint32_t device_count;
            vkEnumeratePhysicalDevices(context.instance, &device_count, nullptr);
            std::vector<VkPhysicalDevice> devices(device_count);
            vkEnumeratePhysicalDevices(context.instance, &device_count, devices.data());

            dtl::log("Vulkan", dtl::severity_info, dtl::type_general, "enumerating physical devices:");
            for (const auto& gpu : devices) {
                VkPhysicalDeviceProperties main_props;
                vkGetPhysicalDeviceProperties(gpu, &main_props);
#if defined(crd_enable_raytracing)
                context.gpu.as_limits.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
                context.gpu.raytracing_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
                context.gpu.raytracing_props.pNext = &context.gpu.as_limits;
                VkPhysicalDeviceProperties2 next_props;
                next_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
                next_props.pNext = &context.gpu.raytracing_props;
                vkGetPhysicalDeviceProperties2(gpu, &next_props);
#endif
                dtl::log("Vulkan", dtl::severity_info, dtl::type_general, "  - found device: %s", main_props.deviceName);
                const auto device_criteria =
                    VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU |
                    VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU   |
                    VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU;
                crd_likely_if(main_props.deviceType & device_criteria) {
                    dtl::log("Vulkan", dtl::severity_info, dtl::type_general, "  - chosen device: %s", main_props.deviceName);
                    const auto driver_major = VK_VERSION_MAJOR(main_props.driverVersion);
                    const auto driver_minor = VK_VERSION_MINOR(main_props.driverVersion);
                    const auto driver_patch = VK_VERSION_PATCH(main_props.driverVersion);
                    const auto vulkan_major = VK_API_VERSION_MAJOR(main_props.apiVersion);
                    const auto vulkan_minor = VK_API_VERSION_MINOR(main_props.apiVersion);
                    const auto vulkan_patch = VK_API_VERSION_PATCH(main_props.apiVersion);
                    dtl::log("Vulkan", dtl::severity_info, dtl::type_general, "  - driver version: %d.%d.%d", driver_major, driver_minor, driver_patch);
                    dtl::log("Vulkan", dtl::severity_info, dtl::type_general, "  - vulkan version: %d.%d.%d", vulkan_major, vulkan_minor, vulkan_patch);
                    context.gpu.main_props = main_props;
                    context.gpu.handle = gpu;
                    break;
                }
            }
            crd_assert(context.gpu.handle, "no suitable GPU found in the system");
        }
        { // Chooses queue families and creates a VkDevice.
            dtl::log("Vulkan", dtl::severity_info, dtl::type_general, "enumerating queue families");
            std::uint32_t families_count;
            vkGetPhysicalDeviceQueueFamilyProperties(context.gpu.handle, &families_count, nullptr);
            std::vector<VkQueueFamilyProperties> queue_families(families_count);
            vkGetPhysicalDeviceQueueFamilyProperties(context.gpu.handle, &families_count, queue_families.data());

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
                crd_force_assert("no suitable graphics queue found");
            }

            if (auto queue = fetch_family(VK_QUEUE_COMPUTE_BIT, VK_QUEUE_GRAPHICS_BIT, 1.0f)) {
                // Prefer a dedicated compute queue which doesn't support graphics.
                families.compute = *queue;
            } else if (auto fallback = fetch_family(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT, 0, 1.0f)) {
                // Fallback to another graphics queue but we can do async graphics that way.
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
            dtl::log("Vulkan", dtl::severity_info, dtl::type_general, "chosen families: %d, %d, %d",
                        families.graphics.family, families.transfer.family, families.compute.family);
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

            dtl::log("Vulkan", dtl::severity_info, dtl::type_general, "fetching device features");
            vkGetPhysicalDeviceFeatures(context.gpu.handle, &context.gpu.features);
            dtl::log("Vulkan", dtl::severity_info, dtl::type_general, "enumerating device extensions");
            std::uint32_t extension_count;
            vkEnumerateDeviceExtensionProperties(context.gpu.handle, nullptr, &extension_count, nullptr);
            std::vector<VkExtensionProperties> extensions(extension_count);
            vkEnumerateDeviceExtensionProperties(context.gpu.handle, nullptr, &extension_count, extensions.data());
            std::vector<const char*> extension_names = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME
            };
            dtl::log("Vulkan", dtl::severity_info, dtl::type_general, "creating device");
            VkPhysicalDeviceDescriptorIndexingFeatures descriptor_indexing = {};
            descriptor_indexing.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
            descriptor_indexing.shaderSampledImageArrayNonUniformIndexing = true;
            descriptor_indexing.descriptorBindingVariableDescriptorCount = true;
            descriptor_indexing.descriptorBindingPartiallyBound = true;
            descriptor_indexing.runtimeDescriptorArray = true;
            VkDeviceCreateInfo device_info;
            device_info.pNext = nullptr;
            if (has_extension(extensions, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME)) {
                extension_names.emplace_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
                context.extensions.descriptor_indexing = true;
                append_to_chain(device_info, descriptor_indexing);
            } else {
                dtl::log("Vulkan", dtl::severity_warning, dtl::type_validation, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME"not available");
            }
            VkPhysicalDeviceBufferDeviceAddressFeatures buffer_address_features = {};
            buffer_address_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
            buffer_address_features.bufferDeviceAddress = true;
            if (has_extension(extensions, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)) {
                extension_names.emplace_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
                context.extensions.buffer_address = true;
                append_to_chain(device_info, buffer_address_features);
            }
#if defined(crd_enable_raytracing)
            VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features = {};
            acceleration_structure_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
            acceleration_structure_features.accelerationStructure = true;
            acceleration_structure_features.descriptorBindingAccelerationStructureUpdateAfterBind = true;
            if (has_extension(extensions, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME)) {
                extension_names.emplace_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
                append_to_chain(device_info, acceleration_structure_features);
            } else {
                dtl::log("Vulkan", dtl::severity_warning, dtl::type_validation, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME" not available");
            }
            VkPhysicalDeviceRayTracingPipelineFeaturesKHR raytracing_features = {};
            raytracing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
            raytracing_features.pNext = nullptr;
            raytracing_features.rayTracingPipeline = true;
            raytracing_features.rayTracingPipelineTraceRaysIndirect = true;
            raytracing_features.rayTraversalPrimitiveCulling = true;
            if (has_extension(extensions, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME)) {
                context.extensions.raytracing = true;
                extension_names.emplace_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
                append_to_chain(device_info, raytracing_features);
            } else {
                dtl::log("Vulkan", dtl::severity_warning, dtl::type_validation, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME" not available");
            }
            if (has_extension(extensions, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME)) {
                extension_names.emplace_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
            } else {
                dtl::log("Vulkan", dtl::severity_warning, dtl::type_validation, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME" not available");
            }
#endif
            device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            device_info.flags = {};
            device_info.pQueueCreateInfos = queue_infos.data();
            device_info.queueCreateInfoCount = queue_infos.size();
            device_info.enabledLayerCount = 0;
            device_info.ppEnabledLayerNames = nullptr;
            device_info.enabledExtensionCount = extension_names.size();
            device_info.ppEnabledExtensionNames = extension_names.data();
            device_info.pEnabledFeatures = &context.gpu.features;
            crd_vulkan_check(vkCreateDevice(context.gpu.handle, &device_info, nullptr, &context.device));
            dtl::log("Vulkan", dtl::severity_info, dtl::type_general, "device created successfully");

            context.graphics = make_queue(context, families.graphics);
            context.transfer = make_queue(context, families.transfer);
            context.compute = make_queue(context, families.compute);
            dtl::log("Vulkan", dtl::severity_info, dtl::type_general, "device queues initialized");
        }
        { // Creates the Task Scheduler.
            dtl::log("Vulkan", dtl::severity_info, dtl::type_general, "initializing task scheduler");
            (context.scheduler = new ftl::TaskScheduler())->Init({
                .Behavior = ftl::EmptyQueueBehavior::Sleep
            });
        }
        { // Creates a Descriptor Pool.
            const auto& limits          = context.gpu.main_props.limits;
            const auto& as_limits       = context.gpu.as_limits;
            const auto max_samplers     = std::min<std::uint32_t>(16384, limits.maxDescriptorSetSampledImages);
            const auto max_uniforms     = std::min<std::uint32_t>(16384, limits.maxDescriptorSetUniformBuffers);
            const auto max_storage      = std::min<std::uint32_t>(16384, limits.maxDescriptorSetStorageBuffers);
            const auto max_images       = std::min<std::uint32_t>(16384, limits.maxDescriptorSetStorageImages);
            const auto max_as           = std::min<std::uint32_t>(16384, as_limits.maxDescriptorSetAccelerationStructures);
            const auto descriptor_sizes = std::to_array<VkDescriptorPoolSize>({
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             max_uniforms },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             max_storage  },
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     max_samplers },
                { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              max_images   },
#if defined(crd_enable_raytracing)
                { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, max_as       },
#endif
            });
            auto total_size = 0;
            for (auto size : descriptor_sizes) {
                total_size += size.descriptorCount;
            }

            VkDescriptorPoolCreateInfo descriptor_pool_info;
            descriptor_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            descriptor_pool_info.pNext = nullptr;
            descriptor_pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            descriptor_pool_info.maxSets = total_size;
            descriptor_pool_info.poolSizeCount = descriptor_sizes.size();
            descriptor_pool_info.pPoolSizes = descriptor_sizes.data();

           crd_vulkan_check(vkCreateDescriptorPool(context.device, &descriptor_pool_info, nullptr, &context.descriptor_pool));
        }
        { // Creates a VkSampler (default and shadow).
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
            sampler_info.mipLodBias = 0.0f;
            sampler_info.anisotropyEnable = true;
            sampler_info.maxAnisotropy = 16;
            sampler_info.compareEnable = false;
            sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
            sampler_info.minLod = 0.0f;
            sampler_info.maxLod = 8.0f;
            sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
            sampler_info.unnormalizedCoordinates = false;
            crd_vulkan_check(vkCreateSampler(context.device, &sampler_info, nullptr, &context.default_sampler));
            sampler_info.anisotropyEnable = false;
            sampler_info.magFilter = VK_FILTER_LINEAR;
            sampler_info.minFilter = VK_FILTER_LINEAR;
            sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
            crd_vulkan_check(vkCreateSampler(context.device, &sampler_info, nullptr, &context.shadow_sampler));
        }
        { // Creates a VmaAllocator.
            dtl::log("Vulkan", dtl::severity_info, dtl::type_general, "creating allocator");
            VmaAllocatorCreateInfo allocator_info;
            allocator_info.flags = {};
            if (context.extensions.buffer_address) {
                allocator_info.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
            }
            allocator_info.physicalDevice = context.gpu.handle;
            allocator_info.device = context.device;
            allocator_info.preferredLargeHeapBlockSize = 0;
            allocator_info.pAllocationCallbacks = nullptr;
            allocator_info.pDeviceMemoryCallbacks = nullptr;
            allocator_info.pHeapSizeLimit = nullptr;
            allocator_info.pVulkanFunctions = nullptr;
            allocator_info.instance = context.instance;
            allocator_info.vulkanApiVersion = VK_API_VERSION_1_2;
            allocator_info.pTypeExternalMemoryHandleTypes = nullptr;
            crd_vulkan_check(vmaCreateAllocator(&allocator_info, &context.allocator));
            dtl::log("Vulkan", dtl::severity_info, dtl::type_general, "allocator created successfully");
        }
        dtl::log("Vulkan", dtl::severity_info, dtl::type_general, "initializing dynamic dispatcher");
        initialize_dynamic_dispatcher(context);
        dtl::log("Vulkan", dtl::severity_info, dtl::type_general, "vulkan initialization completed");
        return context;
    }

    crd_module void destroy_context(Context& context) noexcept {
        dtl::log("Vulkan", dtl::severity_info, dtl::type_general, "terminating core context");
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
        const auto vkDestroyDebugUtilsMessengerEXT = crd_load_instance_function(context.instance, vkDestroyDebugUtilsMessengerEXT);
        vkDestroyDebugUtilsMessengerEXT(context.instance, context.validation, nullptr);
#endif
        vkDestroyInstance(context.instance, nullptr);
        context = {};
        dtl::log("Vulkan", dtl::severity_info, dtl::type_general, "termination complete");
    }

    crd_nodiscard crd_module std::uint32_t max_bound_samplers(const Context& context) noexcept {
        return std::min<std::uint32_t>(context.gpu.main_props.limits.maxPerStageDescriptorSampledImages, 1024);
    }
} //namespace crd
