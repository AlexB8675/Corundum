#include <corundum/core/dispatch.hpp>
#include <corundum/core/context.hpp>

#if defined(crd_enable_profiling)
    #include <Tracy.hpp>
#endif

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

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
        crd_profile_scoped();
        for (const auto& [name, _] : extensions) {
            if (std::strcmp(name, extension) == 0) {
                return true;
            }
        }
        return false;
    }

    template <typename T, typename U>
    static inline void append_to_chain(T& object, U& next) noexcept {
        crd_profile_scoped();
        if (!object.pNext) {
            object.pNext = &next;
            return;
        }
        next.pNext = const_cast<void*>(object.pNext);
        object.pNext = &next;
    }

    static inline void initialize_dynamic_dispatcher(const Context& context) noexcept {
#if defined(crd_enable_raytracing)
        crd_profile_scoped();
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
        crd_profile_scoped();
        Context context = {};
        { // Creates a VkInstance.
            spdlog::info("initializing vulkan 1.2");
            VkApplicationInfo application_info;
            application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            application_info.pNext = nullptr;
            application_info.pApplicationName = "Corundum Engine";
            application_info.applicationVersion = VK_API_VERSION_1_2;
            application_info.pEngineName = "Corundum Engine";
            application_info.engineVersion = VK_API_VERSION_1_2;
            application_info.apiVersion = VK_API_VERSION_1_2;

            spdlog::info("enumerating extensions:");
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
            spdlog::info("initializing validation layer");
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
            spdlog::info("extensions enabled successfully");
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
                spdlog::level::level_enum level;
                switch (severity) {
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: level = spdlog::level::debug; break;
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:    level = spdlog::level::info;  break;
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: level = spdlog::level::warn;  break;
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:   level = spdlog::level::err;   break;

                }
                const char* type_string = nullptr;
                switch (type) {
                    case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:     type_string = "general";     break;
                    case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:  type_string = "validation";  break;
                    case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT: type_string = "performance"; break;
                }

                spdlog::log(level, "[{}]: {}", type_string, data->pMessage);
                const auto fatal_bits = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
                crd_unlikely_if(severity & fatal_bits) {
                    crd_panic();
                }
                return 0;
            };
            validation_info.pUserData = nullptr;

            const auto vkCreateDebugUtilsMessengerEXT = crd_load_instance_function(context.instance, vkCreateDebugUtilsMessengerEXT);
            crd_assert(vkCreateDebugUtilsMessengerEXT, "failed loading validation function");
            crd_vulkan_check(vkCreateDebugUtilsMessengerEXT(context.instance, &validation_info, nullptr, &context.validation));
        }
#endif
        { // Picks a VkPhysicalDevice.
            std::uint32_t device_count;
            vkEnumeratePhysicalDevices(context.instance, &device_count, nullptr);
            std::vector<VkPhysicalDevice> devices(device_count);
            vkEnumeratePhysicalDevices(context.instance, &device_count, devices.data());

            spdlog::info("enumerating physical devices");
            for (const auto gpu : devices) {
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
                spdlog::info("  - found device: {}", main_props.deviceName);
                const auto device_criteria =
                    VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU |
                    VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU   |
                    VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU;
                crd_likely_if(main_props.deviceType & device_criteria) {
                    spdlog::info("  - chosen device: {}", main_props.deviceName);
                    const auto driver_major = VK_VERSION_MAJOR(main_props.driverVersion);
                    const auto driver_minor = VK_VERSION_MINOR(main_props.driverVersion);
                    const auto driver_patch = VK_VERSION_PATCH(main_props.driverVersion);
                    const auto vulkan_major = VK_API_VERSION_MAJOR(main_props.apiVersion);
                    const auto vulkan_minor = VK_API_VERSION_MINOR(main_props.apiVersion);
                    const auto vulkan_patch = VK_API_VERSION_PATCH(main_props.apiVersion);
                    spdlog::info("  - driver version: {}.{}.{}", driver_major, driver_minor, driver_patch);
                    spdlog::info("  - vulkan version: {}.{}.{}", vulkan_major, vulkan_minor, vulkan_patch);
                    context.gpu.main_props = main_props;
                    context.gpu.handle = gpu;
                    break;
                }
            }
            crd_assert(context.gpu.handle, "no suitable GPU found in the system");
        }
        { // Chooses queue families and creates a VkDevice.
            spdlog::info("initializing device queues");
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
            spdlog::info("chosen families: graphics: {}, transfer: {}, compute: {}",
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

            spdlog::info("fetching device features");
            vkGetPhysicalDeviceFeatures(context.gpu.handle, &context.gpu.features);
            spdlog::info("enumerating device extensions");
            std::uint32_t extension_count;
            vkEnumerateDeviceExtensionProperties(context.gpu.handle, nullptr, &extension_count, nullptr);
            std::vector<VkExtensionProperties> extensions(extension_count);
            vkEnumerateDeviceExtensionProperties(context.gpu.handle, nullptr, &extension_count, extensions.data());
            std::vector<const char*> extension_names = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME
            };
            spdlog::info("initializing device");
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
                spdlog::warn(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME" not available");
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
                spdlog::warn(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME" not available");
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
                spdlog::warn(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME" not available");
            }
            if (has_extension(extensions, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME)) {
                extension_names.emplace_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
            } else {
                spdlog::warn(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME" not available");
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

            context.graphics = make_queue(context, families.graphics);
            context.transfer = make_queue(context, families.transfer);
            context.compute = make_queue(context, families.compute);
        }
        { // Creates the Task Scheduler.
            spdlog::info("initializing task scheduler");
            (context.scheduler = new ftl::TaskScheduler())->Init({
                .Behavior = ftl::EmptyQueueBehavior::Sleep
            });
        }
        { // Creates a Descriptor Pool.
            const auto& limits = context.gpu.main_props.limits;
            const auto& as_limits = context.gpu.as_limits;
            const auto max_samplers = std::min<std::uint32_t>(16384, limits.maxDescriptorSetSampledImages);
            const auto max_uniforms = std::min<std::uint32_t>(16384, limits.maxDescriptorSetUniformBuffers);
            const auto max_storage = std::min<std::uint32_t>(16384, limits.maxDescriptorSetStorageBuffers);
            const auto max_images = std::min<std::uint32_t>(16384, limits.maxDescriptorSetStorageImages);
            const auto max_as = std::min<std::uint32_t>(16384, as_limits.maxDescriptorSetAccelerationStructures);
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
        { // Creates a VmaAllocator.
            spdlog::info("initializing allocator");
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
        }
        spdlog::info("initializing dynamic dispatch");
        initialize_dynamic_dispatcher(context);
        spdlog::info("initialization completed");
        return context;
    }

    crd_module void destroy_context(Context& context) noexcept {
        crd_profile_scoped();
        spdlog::info("terminating core context");
        delete context.scheduler;
        destroy_queue(context, context.graphics);
        destroy_queue(context, context.transfer);
        destroy_queue(context, context.compute);
        vkDestroyDescriptorPool(context.device, context.descriptor_pool, nullptr);
        vmaDestroyAllocator(context.allocator);
        vkDestroyDevice(context.device, nullptr);
#if defined(crd_debug)
        const auto vkDestroyDebugUtilsMessengerEXT = crd_load_instance_function(context.instance, vkDestroyDebugUtilsMessengerEXT);
        vkDestroyDebugUtilsMessengerEXT(context.instance, context.validation, nullptr);
#endif
        vkDestroyInstance(context.instance, nullptr);
        context = {};
        spdlog::info("termination completed");
    }

    crd_nodiscard crd_module std::uint32_t max_bound_samplers(const Context& context) noexcept {
        crd_profile_scoped();
        return std::min<std::uint32_t>(context.gpu.main_props.limits.maxPerStageDescriptorSampledImages, 1024);
    }
} //namespace crd
