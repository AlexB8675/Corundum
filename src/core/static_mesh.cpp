#include <corundum/core/command_buffer.hpp>
#include <corundum/core/static_mesh.hpp>
#include <corundum/core/utilities.hpp>
#include <corundum/core/dispatch.hpp>
#include <corundum/core/context.hpp>
#include <corundum/core/async.hpp>
#include <corundum/core/queue.hpp>

#if defined(crd_enable_profiling)
    #include <Tracy.hpp>
#endif

#include <spdlog/spdlog.h>

#include <cstring>

namespace crd {
    crd_nodiscard crd_module Async<StaticMesh> request_static_mesh(const Context& context, StaticMesh::CreateInfo&& info) noexcept {
        crd_profile_scoped();
        using task_type = std::packaged_task<StaticMesh(ftl::TaskScheduler*)>;
        auto task = new task_type([&context, info = std::move(info)](ftl::TaskScheduler* scheduler) noexcept -> StaticMesh {
            crd_profile_scoped();
            const auto thread_index = scheduler->GetCurrentThreadIndex();
            const auto graphics_pool = context.graphics->transient[thread_index];
            const auto transfer_pool = context.transfer->transient[thread_index];
            const auto vertex_bytes = size_bytes(info.geometry);
            const auto index_bytes = size_bytes(info.indices);
            spdlog::info("StaticMesh was asynchronously requested, expected bytes to transfer: {}", vertex_bytes + index_bytes);
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
            VkSemaphoreCreateInfo semaphore_info;
            semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            semaphore_info.pNext = nullptr;
            semaphore_info.flags = {};
            VkSemaphore transfer_done;
            crd_vulkan_check(vkCreateSemaphore(context.device, &semaphore_info, nullptr, &transfer_done));
            context.transfer->submit({
                .commands = transfer_cmd,
                .stages = {},
                .waits = {},
                .signals = { transfer_done },
                .done = {}
            });
            auto ownership_cmd = make_command_buffer(context, {
                .pool = graphics_pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            });
            ownership_cmd
                .begin()
#if defined(crd_enable_raytracing)
                .transfer_ownership({
                    .buffer = &geometry,
                    .source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    .dest_stage = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                    .source_access = {},
                    .dest_access = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR
                }, *context.transfer, *context.graphics)
                .transfer_ownership({
                    .buffer = &indices,
                    .source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    .dest_stage = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                    .source_access = {},
                    .dest_access = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR
                }, *context.transfer, *context.graphics)
#else
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
#endif
                .end();
            VkFenceCreateInfo fence_info;
            fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fence_info.pNext = nullptr;
            fence_info.flags = {};
            VkFence request_done;
            crd_vulkan_check(vkCreateFence(context.device, &fence_info, nullptr, &request_done));
            context.graphics->submit({
                .commands = ownership_cmd,
                .stages = { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT },
                .waits = { transfer_done },
                .signals = {},
                .done = request_done
            });
            wait_fence(context, request_done);
            StaticMesh result;
            result.context = &context;
            result.geometry = geometry;
            result.indices = indices;
#if defined(crd_enable_raytracing)
            // BLAS
            {
                const auto triangles = (std::uint32_t)info.indices.size() / 3;

                VkAccelerationStructureGeometryKHR as_geometry = {};
                as_geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
                as_geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
                as_geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
                as_geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
                as_geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
                as_geometry.geometry.triangles.vertexData.deviceAddress = geometry.address;
                as_geometry.geometry.triangles.maxVertex = vertex_bytes / vertex_size;
                as_geometry.geometry.triangles.vertexStride = vertex_size;
                as_geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
                as_geometry.geometry.triangles.indexData.deviceAddress = indices.address;

                VkAccelerationStructureBuildGeometryInfoKHR as_build_info = {};
                as_build_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
                as_build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
                as_build_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
                as_build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
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

                spdlog::info("creating BLAS, requesting: {} bytes", as_build_sizes.accelerationStructureSize);
                result.blas.buffer = make_static_buffer(context, {
                    .flags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
                    .usage = VMA_MEMORY_USAGE_GPU_ONLY,
                    .capacity = as_build_sizes.accelerationStructureSize
                });

                VkAccelerationStructureCreateInfoKHR as_info = {};
                as_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
                as_info.createFlags = {};
                as_info.buffer = result.blas.buffer.handle;
                as_info.offset = 0;
                as_info.size = result.blas.buffer.capacity;
                as_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
                as_info.deviceAddress = 0;
                crd_vulkan_check(vkCreateAccelerationStructureKHR(context.device, &as_info, nullptr, &result.blas.handle));
                result.blas.address = device_address(context, result.blas);

                spdlog::info("building BLAS, requesting: %llu bytes", as_build_sizes.buildScratchSize);
                auto build_scratch_buffer = make_static_buffer(context, {
                    .flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    .usage = VMA_MEMORY_USAGE_GPU_ONLY,
                    .capacity = as_build_sizes.buildScratchSize
                });

                as_build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
                as_build_info.dstAccelerationStructure = result.blas.handle;
                as_build_info.scratchData.deviceAddress = device_address(context, build_scratch_buffer);

                VkAccelerationStructureBuildRangeInfoKHR as_build_range;
                as_build_range.primitiveCount = triangles;
                as_build_range.primitiveOffset = 0;
                as_build_range.firstVertex = 0;
                as_build_range.transformOffset = 0;
                auto build_blas_commands = make_command_buffer(context, {
                    .pool = graphics_pool,
                    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY
                });
                build_blas_commands
                    .begin()
                    .build_acceleration_structure(&as_build_info, &as_build_range)
                    .memory_barrier({
                        .source_stage = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                        .dest_stage = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                        .source_access = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                        .dest_access = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
                    })
                    .end();
                immediate_submit(context, build_blas_commands, queue_type_graphics);
                build_scratch_buffer.destroy();
                destroy_command_buffer(context, build_blas_commands);
            }
#else
            wait_fence(context, request_done);
            vkDestroyFence(context.device, request_done, nullptr);
#endif
            vkDestroySemaphore(context.device, transfer_done, nullptr);
            vertex_staging.destroy();
            index_staging.destroy();
            destroy_command_buffer(context, ownership_cmd);
            destroy_command_buffer(context, transfer_cmd);
            return result;
        });
        auto future = task->get_future();
        context.scheduler->AddTask({
            .Function = [](ftl::TaskScheduler* scheduler, void* data) {
                crd_profile_scoped();
                auto task = static_cast<task_type*>(data);
                (*task)(scheduler);
                delete task;
            },
            .ArgData = task
        }, ftl::TaskPriority::High);
        return make_async(std::move(future));
    }

    crd_module void StaticMesh::destroy() noexcept {
        crd_profile_scoped();
        geometry.destroy();
        indices.destroy();
        *this = {};
    }
} // namespace crd
