#include <corundum/core/static_mesh.hpp>
#include <corundum/core/render_pass.hpp>
#include <corundum/core/swapchain.hpp>
#include <corundum/core/constants.hpp>
#include <corundum/core/renderer.hpp>
#include <corundum/core/pipeline.hpp>
#include <corundum/core/context.hpp>
#include <corundum/core/clear.hpp>
#include <corundum/core/async.hpp>

#include <corundum/wm/window.hpp>

#include <algorithm>

int main() {
    auto window      = crd::wm::make_window(1280, 720, "Sorting Algos");
    auto context     = crd::core::make_context();
    auto renderer    = crd::core::make_renderer(context);
    auto swapchain   = crd::core::make_swapchain(context, window);
    auto render_pass = crd::core::make_render_pass(context, {
        .attachments = { {
            .image = crd::core::make_image(context, {
                .width   = 1280,
                .height  = 720,
                .mips    = 1,
                .format  = swapchain.format,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .usage   = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT
            }),
            .layout = {
                .initial = VK_IMAGE_LAYOUT_UNDEFINED,
                .final   = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
            },
            .clear   = crd::core::make_clear_color({
                24  / 255.0f,
                154 / 255.0f,
                207 / 255.0f,
                1.0f
            }),
            .owning  = true,
            .discard = false
        }, {
            .image = crd::core::make_image(context, {
                .width   = 1280,
                .height  = 720,
                .mips    = 1,
                .format  = VK_FORMAT_D32_SFLOAT_S8_UINT,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .usage   = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
            }),
            .layout = {
                .initial = VK_IMAGE_LAYOUT_UNDEFINED,
                .final   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            },
            .clear   = crd::core::make_clear_depth({ 1.0, 0 }),
            .owning  = true,
            .discard = true
        } },
        .subpasses = { {
            .attachments = { 0, 1 },
            .preserve    = {},
            .input       = {}
        } },
        .dependencies = { {
            .source_subpass = crd::core::external_subpass,
            .dest_subpass   = 0,
            .source_stage   = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dest_stage     = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .source_access  = {},
            .dest_access    = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
        } },
        .framebuffers = {
            { { 0, 1 } }
        }
    });
    auto pipeline = crd::core::make_pipeline(context, renderer, {
        .vertex = "data/shaders/main.vert.spv",
        .fragment = "data/shaders/main.frag.spv",
        .render_pass = &render_pass,
        .attributes = {
            crd::core::vertex_attribute_vec3,
            crd::core::vertex_attribute_vec3
        },
        .states = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        },
        .subpass = 0,
        .depth = true
    });
    auto triangle = crd::core::request_static_mesh(context, {
        .geometry = {
            -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f,
             0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f,
             0.0f,  0.5f, 0.0f, 0.0f, 0.0f, 1.0f
        },
        .indices = {
            0, 1, 2
        }
    });

    while (!crd::wm::is_closed(window)) {
        const auto [commands, image, index] = renderer.acquire_frame(context, swapchain);
        commands
            .begin()
            .begin_render_pass(render_pass, 0)
            .set_viewport(0)
            .set_scissor(0)
            .bind_pipeline(pipeline);
        if (triangle.is_ready()) {
            commands
                .bind_static_mesh(triangle.get())
                .draw_indexed(3, 1, 0, 0, 0);
        }
        commands
            .end_render_pass()
            .insert_layout_transition({
                .image = &image,
                .mip = 0,
                .source_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dest_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .source_access = {},
                .dest_access = VK_ACCESS_TRANSFER_WRITE_BIT,
                .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                .new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            })
            .copy_image(render_pass.image(0), image)
            .insert_layout_transition({
                .image = &image,
                .mip = 0,
                .source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .dest_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                .source_access = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dest_access = {},
                .old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .new_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
            })
            .end();
        renderer.present_frame(context, swapchain, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        crd::wm::poll_events();
    }
    context.graphics->wait_idle();
    crd::core::destroy_static_mesh(context, triangle.get());
    crd::core::destroy_pipeline(context, pipeline);
    crd::core::destroy_render_pass(context, render_pass);
    crd::core::destroy_swapchain(context, swapchain);
    crd::core::destroy_renderer(context, renderer);
    crd::core::destroy_context(context);
    crd::wm::destroy_window(window);
    return 0;
}
