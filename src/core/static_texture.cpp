#include <corundum/core/static_texture.hpp>
#include <corundum/core/command_buffer.hpp>
#include <corundum/core/static_buffer.hpp>
#include <corundum/core/renderer.hpp>
#include <corundum/core/context.hpp>
#include <corundum/core/async.hpp>
#include <corundum/core/queue.hpp>

#include <corundum/detail/file_view.hpp>

#if defined(crd_enable_profiling)
    #include <Tracy.hpp>
#endif

#include <spdlog/spdlog.h>

#include <vulkan/vulkan.h>

#include <stb_image.h>

#include <cstring>
#include <future>
#include <cmath>

namespace crd {
    crd_nodiscard crd_module Async<StaticTexture> request_static_texture(Renderer& renderer, std::string&& path, TextureFormat format) noexcept {
        crd_profile_scoped();
        using task_type = std::packaged_task<StaticTexture(ftl::TaskScheduler*)>;
        const auto* context = renderer.context;
        auto task = new task_type([context, &renderer, path = std::move(path), format](ftl::TaskScheduler* scheduler) noexcept -> StaticTexture {
            crd_profile_scoped();
            const auto thread_index = scheduler->GetCurrentThreadIndex();
            const auto graphics_pool = context->graphics->transient[thread_index];
            const auto transfer_pool = context->transfer->transient[thread_index];
            std::int32_t width, height, channels = 4;
            auto file = dtl::make_file_view(path.c_str());
            auto image_data = stbi_load_from_memory(static_cast<const std::uint8_t*>(file.data), file.size, &width, &height, &channels, STBI_rgb_alpha);
            if (!image_data) {
                spdlog::info("error loading texture: {}", path);
            }
            spdlog::info("StaticTexture was asynchronously requested, expected bytes to transfer: {}", file.size);
            dtl::destroy_file_view(file);
            auto image = make_image(*context, {
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
            auto staging = make_static_buffer(*context, {
                .flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .usage = VMA_MEMORY_USAGE_CPU_ONLY,
                .capacity = (std::size_t)width * height * 4
            });
            std::memcpy(staging.mapped, image_data, staging.capacity);
            stbi_image_free(image_data);

            auto transfer_cmd = make_command_buffer(*context, {
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
                }, *context->transfer, *context->graphics)
                .end();

            VkSemaphoreCreateInfo semaphore_info;
            semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            semaphore_info.pNext = nullptr;
            semaphore_info.flags = {};
            VkSemaphore transfer_done;
            crd_vulkan_check(vkCreateSemaphore(context->device, &semaphore_info, nullptr, &transfer_done));
            context->transfer->submit({
                .commands = transfer_cmd,
                .stages = {},
                .waits = {},
                .signals = { transfer_done },
                .done = {}
            });
            auto ownership_cmd = make_command_buffer(*context, {
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
                }, *context->transfer, *context->graphics);
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
            VkPipelineStageFlags final_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
#if defined(crd_enable_raytracing)
            final_stage |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
#endif
            ownership_cmd
                .transition_layout({
                    .image = &image,
                    .mip = 0,
                    .level = std::max(image.mips - 1, 1u),
                    .source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                    .dest_stage = final_stage,
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
            crd_vulkan_check(vkCreateFence(context->device, &done_fence_info, nullptr, &request_done));
            context->graphics->submit({
                .commands = ownership_cmd,
                .stages = { VK_PIPELINE_STAGE_TRANSFER_BIT },
                .waits = { transfer_done },
                .signals = {},
                .done = request_done
            });
            wait_fence(*context, request_done);
            vkDestroySemaphore(context->device, transfer_done, nullptr);
            vkDestroyFence(context->device, request_done, nullptr);
            staging.destroy();
            destroy_command_buffer(*context, ownership_cmd);
            destroy_command_buffer(*context, transfer_cmd);
            return {
                image, renderer.acquire_sampler({
                    .filter = VK_FILTER_LINEAR,
                    .border_color = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
                    .address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                    .anisotropy = 16,
                })
            };
        });
        auto future = task->get_future();
        context->scheduler->AddTask({
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

    crd_nodiscard crd_module VkDescriptorImageInfo StaticTexture::info() const noexcept {
        crd_profile_scoped();
        return image.sample(sampler);
    }

    crd_module void StaticTexture::destroy() noexcept {
        crd_profile_scoped();
        image.destroy();
        *this = {};
    }
} // namespace crd
