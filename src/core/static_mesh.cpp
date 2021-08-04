#include <corundum/core/command_buffer.hpp>
#include <corundum/core/static_mesh.hpp>
#include <corundum/core/context.hpp>
#include <corundum/core/async.hpp>
#include <corundum/core/queue.hpp>

#include <corundum/detail/logger.hpp>

#include <cstring>

namespace crd {
    crd_nodiscard crd_module Async<StaticMesh> request_static_mesh(const Context& context, StaticMesh::CreateInfo&& info) noexcept {
        const auto task = new std::packaged_task<StaticMesh(ftl::TaskScheduler*)>(
            [&context, info = std::move(info)](ftl::TaskScheduler* scheduler) noexcept -> StaticMesh {
                const auto thread_index  = scheduler->GetCurrentThreadIndex();
                const auto graphics_pool = context.graphics->transient[thread_index];
                const auto transfer_pool = context.transfer->transient[thread_index];
                const auto vertex_bytes  = info.geometry.size() * sizeof(float);
                const auto index_bytes   = info.indices.size() * sizeof(std::uint32_t);
                detail::log("Vulkan", detail::Severity::eInfo, detail::Type::eGeneral,
                          "StaticMesh was asynchronously requested, expected bytes to transfer: %zu", vertex_bytes * index_bytes);
                auto vertex_staging = make_static_buffer(context, {
                    .flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    .usage = VMA_MEMORY_USAGE_CPU_ONLY,
                    .capacity = vertex_bytes
                });
                std::memcpy(vertex_staging.mapped, info.geometry.data(), vertex_staging.capacity);
                auto index_staging = make_static_buffer(context, {
                    .flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    .usage = VMA_MEMORY_USAGE_CPU_ONLY,
                    .capacity = index_bytes
                });
                std::memcpy(index_staging.mapped, info.indices.data(), index_staging.capacity);
                auto geometry = make_static_buffer(context, {
                    .flags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    .usage = VMA_MEMORY_USAGE_GPU_ONLY,
                    .capacity = vertex_bytes
                });
                auto indices = make_static_buffer(context, {
                    .flags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT,
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
                context.transfer->submit(transfer_cmd, {}, nullptr, transfer_done, nullptr);

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
                VkFenceCreateInfo done_fence_info;
                done_fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                done_fence_info.pNext = nullptr;
                done_fence_info.flags = {};
                VkFence request_done;
                crd_vulkan_check(vkCreateFence(context.device, &done_fence_info, nullptr, &request_done));
                context.graphics->submit(ownership_cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, transfer_done, nullptr, request_done);
                vkWaitForFences(context.device, 1, &request_done, true, -1);
                vkDestroySemaphore(context.device, transfer_done, nullptr);
                vkDestroyFence(context.device, request_done, nullptr);
                destroy_static_buffer(context, vertex_staging);
                destroy_static_buffer(context, index_staging);
                destroy_command_buffer(context, ownership_cmd);
                destroy_command_buffer(context, transfer_cmd);
                return {
                    geometry,
                    indices
                };
            });
        Async<StaticMesh> resource;
        resource.future = task->get_future();
        context.scheduler->AddTask({
            .Function = [](ftl::TaskScheduler* scheduler, void* data) {
                auto* task = static_cast<std::packaged_task<StaticMesh(ftl::TaskScheduler*)>*>(data);
                (*task)(scheduler);
                delete task;
            },
            .ArgData = task
        }, ftl::TaskPriority::High);
        return resource;
    }

    crd_module void destroy_static_mesh(const Context& context, StaticMesh& mesh) noexcept {
        destroy_static_buffer(context, mesh.geometry);
        destroy_static_buffer(context, mesh.indices);
        mesh = {};
    }
} // namespace crd
