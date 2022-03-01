#include <corundum/core/command_buffer.hpp>
#include <corundum/core/static_mesh.hpp>
#include <corundum/core/utilities.hpp>
#include <corundum/core/dispatch.hpp>
#include <corundum/core/context.hpp>
#include <corundum/core/async.hpp>
#include <corundum/core/queue.hpp>

#include <corundum/detail/logger.hpp>

#include <cstring>

namespace crd {
    crd_nodiscard crd_module Async<StaticMesh> request_static_mesh(const Context& context, StaticMesh::CreateInfo&& info) noexcept {
        using task_type = std::packaged_task<StaticMesh(ftl::TaskScheduler*)>;
        auto* task = new task_type([&context, info = std::move(info)](ftl::TaskScheduler* scheduler) noexcept -> StaticMesh {
            const auto thread_index  = scheduler->GetCurrentThreadIndex();
            const auto graphics_pool = context.graphics->transient[thread_index];
            const auto transfer_pool = context.transfer->transient[thread_index];
            const auto vertex_bytes  = size_bytes(info.geometry);
            const auto index_bytes   = size_bytes(info.indices);
            detail::log("Vulkan", detail::severity_verbose, detail::type_general,
                        "StaticMesh was asynchronously requested, expected bytes to transfer: %zu", vertex_bytes * index_bytes);
            auto vertex_staging = make_static_buffer(context, {
                .flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .usage = VMA_MEMORY_USAGE_CPU_ONLY,
                .capacity = vertex_bytes
            });
            auto index_staging = make_static_buffer(context, {
                .flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .usage = VMA_MEMORY_USAGE_CPU_ONLY,
                .capacity = index_bytes
            });
            std::memcpy(vertex_staging.mapped, info.geometry.data(), vertex_staging.capacity);
            std::memcpy(index_staging.mapped, info.indices.data(), index_staging.capacity);
            VkBufferUsageFlags buffer_usages = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                               VK_BUFFER_USAGE_TRANSFER_DST_BIT;
#if defined(crd_enable_raytracing)
            buffer_usages |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
#endif
            auto geometry = make_static_buffer(context, {
                .flags = buffer_usages,
                .usage = VMA_MEMORY_USAGE_GPU_ONLY,
                .capacity = vertex_bytes
            });
            buffer_usages = VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                            VK_BUFFER_USAGE_TRANSFER_DST_BIT;
#if defined(crd_enable_raytracing)
            buffer_usages |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
#endif
            auto indices = make_static_buffer(context, {
                .flags = buffer_usages,
                .usage = VMA_MEMORY_USAGE_GPU_ONLY,
                .capacity = index_bytes
            });
            auto transfer_cmd = make_command_buffer(context, {
                .pool = transfer_pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            });
            transfer_cmd
                .begin()
                .copy_buffer(vertex_staging, geometry)
                .copy_buffer(index_staging, indices)
                .transfer_ownership({
                    .buffer = &geometry,
                    .source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                    .dest_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                    .source_access = VK_ACCESS_TRANSFER_WRITE_BIT,
                    .dest_access = {}
                }, *context.transfer, *context.graphics)
                .transfer_ownership({
                    .buffer = &indices,
                    .source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                    .dest_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                    .source_access = VK_ACCESS_TRANSFER_WRITE_BIT,
                    .dest_access = {}
                }, *context.transfer, *context.graphics)
                .end();
            VkSemaphoreCreateInfo transfer_semaphore_info;
            transfer_semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            transfer_semaphore_info.pNext = nullptr;
            transfer_semaphore_info.flags = {};
            VkSemaphore transfer_done;
            crd_vulkan_check(vkCreateSemaphore(context.device, &transfer_semaphore_info, nullptr, &transfer_done));
            context.transfer->submit({
                .commands = transfer_cmd,
                .stages   = {},
                .waits    = {},
                .signals  = { transfer_done },
                .done     = {}
            });
            auto ownership_cmd = make_command_buffer(context, {
                .pool = graphics_pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            });
            ownership_cmd
                .begin()
                .transfer_ownership({
                    .buffer = &geometry,
                    .source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    .dest_stage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                    .source_access = {},
                    .dest_access = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
                }, *context.transfer, *context.graphics)
                .transfer_ownership({
                    .buffer = &indices,
                    .source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    .dest_stage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                    .source_access = {},
                    .dest_access = VK_ACCESS_INDEX_READ_BIT
                }, *context.transfer, *context.graphics)
                .end();
            VkFenceCreateInfo fence_info;
            fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fence_info.pNext = nullptr;
            fence_info.flags = {};
            VkFence request_done;
            crd_vulkan_check(vkCreateFence(context.device, &fence_info, nullptr, &request_done));
            context.graphics->submit({
                .commands = ownership_cmd,
                .stages   = { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT },
                .waits    = { transfer_done },
                .signals  = {},
                .done     = request_done
            });
            vkWaitForFences(context.device, 1, &request_done, true, -1);
            StaticMesh result;
            result.geometry = geometry;
            result.indices = indices;
#if defined(crd_enable_raytracing)
            VkBufferDeviceAddressInfo buffer_address;
            buffer_address.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            buffer_address.pNext = nullptr;
            buffer_address.buffer = geometry.handle;
            const auto vertex_address = vkGetBufferDeviceAddress(context.device, &buffer_address);
            buffer_address.buffer = indices.handle;
            const auto index_address = vkGetBufferDeviceAddress(context.device, &buffer_address);
            const auto triangles = (std::uint32_t)info.indices.size() / 3;

            VkAccelerationStructureGeometryKHR as_geometry = {};
            as_geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            as_geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
            as_geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            as_geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            as_geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
            as_geometry.geometry.triangles.vertexData.deviceAddress = vertex_address;
            as_geometry.geometry.triangles.maxVertex = vertex_bytes / vertex_size;
            as_geometry.geometry.triangles.vertexStride = vertex_size;
            as_geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
            as_geometry.geometry.triangles.indexData.deviceAddress = index_address;

            VkAccelerationStructureBuildGeometryInfoKHR as_build_info = {};
            as_build_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
            as_build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            as_build_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                                  VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
            as_build_info.geometryCount = 1;
            as_build_info.pGeometries = &as_geometry;

            VkAccelerationStructureBuildSizesInfoKHR as_build_sizes = {};
            as_build_sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
            vkGetAccelerationStructureBuildSizesKHR(
                context.device,
                VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                &as_build_info,
                &triangles,
                &as_build_sizes);

            detail::log("Vulkan", detail::severity_info, detail::type_general, "creating BLAS, requesting: %llu bytes", as_build_sizes.accelerationStructureSize);
            auto create_scratch_buffer = make_static_buffer(context, {
                .flags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .usage = VMA_MEMORY_USAGE_GPU_ONLY,
                .capacity = as_build_sizes.accelerationStructureSize
            });

            buffer_address.buffer = create_scratch_buffer.handle;
            VkAccelerationStructureCreateInfoKHR as_info = {};
            as_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
            as_info.createFlags = {};
            as_info.buffer = create_scratch_buffer.handle;
            as_info.offset = 0;
            as_info.size = create_scratch_buffer.capacity;
            as_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            as_info.deviceAddress = 0;
            crd_vulkan_check(vkCreateAccelerationStructureKHR(context.device, &as_info, nullptr, &result.blas.handle));
            result.blas.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

            detail::log("Vulkan", detail::severity_info, detail::type_general, "building BLAS, requesting: %llu bytes", as_build_sizes.buildScratchSize);
            auto build_scratch_buffer = make_static_buffer(context, {
                .flags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .usage = VMA_MEMORY_USAGE_GPU_ONLY,
                .capacity = as_build_sizes.buildScratchSize
            });
            buffer_address.buffer = build_scratch_buffer.handle;
            as_build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
            as_build_info.dstAccelerationStructure = result.blas.handle;
            as_build_info.scratchData.deviceAddress = vkGetBufferDeviceAddress(context.device, &buffer_address);

            VkAccelerationStructureBuildRangeInfoKHR as_build_range;
            as_build_range.primitiveCount = triangles;
            as_build_range.primitiveOffset = 0;
            as_build_range.firstVertex = 0;
            as_build_range.transformOffset = 0;
            auto build_blas_commands = crd::make_command_buffer(context, {
                .pool = graphics_pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY
            });
            build_blas_commands
                .begin()
                .build_acceleration_structure(&as_build_info, &as_build_range)
                .end();
            VkFence build_blas_done;
            crd_vulkan_check(vkCreateFence(context.device, &fence_info, nullptr, &build_blas_done));
            context.graphics->submit({
                .commands = build_blas_commands,
                .stages   = {},
                .waits    = {},
                .signals  = {},
                .done     = build_blas_done
            });
            vkWaitForFences(context.device, 1, &build_blas_done, true, -1);
            vkDestroyFence(context.device, build_blas_done, nullptr);
            destroy_static_buffer(context, create_scratch_buffer);
            destroy_static_buffer(context, build_scratch_buffer);
            destroy_command_buffer(context, build_blas_commands);
#endif
            vkDestroySemaphore(context.device, transfer_done, nullptr);
            vkDestroyFence(context.device, request_done, nullptr);
            destroy_static_buffer(context, vertex_staging);
            destroy_static_buffer(context, index_staging);
            destroy_command_buffer(context, ownership_cmd);
            destroy_command_buffer(context, transfer_cmd);
            return result;
        });
        context.scheduler->AddTask({
            .Function = [](ftl::TaskScheduler* scheduler, void* data) {
                auto* task = static_cast<task_type*>(data);
                crd_benchmark("time took to upload StaticMesh resource: %llums", *task, scheduler);
                delete task;
            },
            .ArgData = task
        }, ftl::TaskPriority::High);
        return make_async(task->get_future());
    }

    crd_module void destroy_static_mesh(const Context& context, StaticMesh& mesh) noexcept {
        destroy_static_buffer(context, mesh.geometry);
        destroy_static_buffer(context, mesh.indices);
        mesh = {};
    }
} // namespace crd
