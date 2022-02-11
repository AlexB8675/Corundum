#include <corundum/core/static_texture.hpp>
#include <corundum/core/command_buffer.hpp>
#include <corundum/core/static_buffer.hpp>
#include <corundum/core/context.hpp>
#include <corundum/core/async.hpp>
#include <corundum/core/queue.hpp>

#include <corundum/detail/file_view.hpp>
#include <corundum/detail/logger.hpp>

#include <vulkan/vulkan.h>

#include <stb_image.h>

#include <cstring>
#include <future>
#include <cmath>

namespace crd {
    crd_nodiscard crd_module Async<StaticTexture> request_static_texture(const Context& context, std::string&& path, TextureFormat format) noexcept {
        using task_type = std::packaged_task<StaticTexture(ftl::TaskScheduler*)>;
        auto* task = new task_type([&context, path = std::move(path), format](ftl::TaskScheduler* scheduler) noexcept -> StaticTexture {
            const auto thread_index  = scheduler->GetCurrentThreadIndex();
            const auto graphics_pool = context.graphics->transient[thread_index];
            const auto transfer_pool = context.transfer->transient[thread_index];
            std::int32_t width, height, channels = 4;
            auto  file = detail::make_file_view(path.c_str());
            auto* image_data = stbi_load_from_memory(static_cast<const std::uint8_t*>(file.data), file.size,
                                                     &width, &height, &channels, STBI_rgb_alpha);
            detail::log("Vulkan", detail::severity_verbose, detail::type_general,
                        "StaticTexture was asynchronously requested, expected bytes to transfer: %zu", file.size);
            detail::destroy_file_view(file);
            auto image = make_image(context, {
                .width = (std::uint32_t)width,
                .height = (std::uint32_t)height,
                .mips = (std::uint32_t)std::floor(std::log2(std::max(width, height))) + 1,
                .layers = 1,
                .format = static_cast<VkFormat>(format),
                .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                         VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                         VK_IMAGE_USAGE_SAMPLED_BIT
            });
            auto staging = make_static_buffer(context, {
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VMA_MEMORY_USAGE_CPU_ONLY,
                (std::size_t)width * height * 4
            });
            std::memcpy(staging.mapped, image_data, staging.capacity);
            stbi_image_free(image_data);

            auto transfer_cmd = make_command_buffer(context, {
                .pool = transfer_pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY
            });
            transfer_cmd
                .begin()
                .transition_layout({
                    .image = &image,
                    .mip = 0,
                    .source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    .dest_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                    .source_access = {},
                    .dest_access = VK_ACCESS_TRANSFER_WRITE_BIT,
                    .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                })
                .copy_buffer_to_image(staging, image)
                .transfer_ownership({
                    .image = &image,
                    .mip = 0,
                    .level = 0,
                    .source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                    .dest_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                    .source_access = VK_ACCESS_TRANSFER_WRITE_BIT,
                    .dest_access = {},
                    .old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .new_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
                }, *context.transfer, *context.graphics)
                .end();

            VkSemaphoreCreateInfo semaphore_info;
            semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            semaphore_info.pNext = nullptr;
            semaphore_info.flags = {};
            VkSemaphore transfer_done;
            crd_vulkan_check(vkCreateSemaphore(context.device, &semaphore_info, nullptr, &transfer_done));
            context.transfer->submit(transfer_cmd, {}, nullptr, transfer_done, nullptr);
            auto ownership_cmd = make_command_buffer(context, {
                .pool = graphics_pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY
            });
            ownership_cmd
                .begin()
                .transfer_ownership({
                    .image = &image,
                    .mip = 0,
                    .level = 0,
                    .source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                    .dest_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                    .source_access = {},
                    .dest_access = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                    .old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .new_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
                }, *context.transfer, *context.graphics);
            for (std::uint32_t mip = 1; mip < image.mips; ++mip) {
                ownership_cmd
                    .transition_layout({
                        .image = &image,
                        .mip = mip,
                        .level = 1,
                        .source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                        .dest_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                        .source_access = {},
                        .dest_access = VK_ACCESS_TRANSFER_WRITE_BIT,
                        .old_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        .new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                    })
                    .blit_image({
                        .source_image = &image,
                        .dest_image = nullptr,
                        .source_off = {
                            (std::int32_t)image.width >> (mip - 1),
                            (std::int32_t)image.height >> (mip - 1),
                            1
                        },
                        .dest_off = {
                            (std::int32_t)image.width >> mip,
                            (std::int32_t)image.height >> mip,
                            1
                        },
                        .source_mip = mip - 1,
                        .dest_mip = mip
                    });
                if (mip != image.mips - 1) {
                    ownership_cmd.transition_layout({
                        .image = &image,
                        .mip = mip,
                        .level = 1,
                        .source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                        .dest_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                        .source_access = VK_ACCESS_TRANSFER_WRITE_BIT,
                        .dest_access = VK_ACCESS_TRANSFER_READ_BIT,
                        .old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        .new_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
                    });
                } else {
                    ownership_cmd.transition_layout({
                        .image = &image,
                        .mip = mip,
                        .level = 1,
                        .source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                        .dest_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        .source_access = VK_ACCESS_TRANSFER_WRITE_BIT,
                        .dest_access = VK_ACCESS_SHADER_READ_BIT,
                        .old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    });
                }
            }
            ownership_cmd
                .transition_layout({
                    .image = &image,
                    .mip = 0,
                    .level = std::max(image.mips - 1, 1u),
                    .source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                    .dest_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    .source_access = VK_ACCESS_TRANSFER_READ_BIT,
                    .dest_access = VK_ACCESS_SHADER_READ_BIT,
                    .old_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                })
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
            destroy_static_buffer(context, staging);
            destroy_command_buffer(context, ownership_cmd);
            destroy_command_buffer(context, transfer_cmd);
            return {
                image,
                context.default_sampler
            };
        });
        Async<StaticTexture> resource;
        resource.import(task->get_future());
        context.scheduler->AddTask({
            .Function = [](ftl::TaskScheduler* scheduler, void* data) {
                auto* task = static_cast<task_type*>(data);
                crd_benchmark("time took to upload StaticTexture resource: %llums", *task, scheduler);
                delete task;
            },
            .ArgData = task
        }, ftl::TaskPriority::High);
        return resource;
    }

    crd_module void destroy_static_texture(const Context& context, StaticTexture& texture) {
        destroy_image(context, texture.image);
        texture = {};
    }

    crd_nodiscard crd_module VkDescriptorImageInfo StaticTexture::info() const noexcept {
        return {
            .sampler = sampler,
            .imageView = image.view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
    }
} // namespace crd
