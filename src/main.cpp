#include <corundum/core/render_pass.hpp>
#include <corundum/core/swapchain.hpp>
#include <corundum/core/constants.hpp>
#include <corundum/core/context.hpp>
#include <corundum/core/clear.hpp>

#include <corundum/wm/window.hpp>

int main() {
    auto window      = crd::wm::make_window(1280, 720, "Sorting Algos");
    auto context     = crd::core::make_context();
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
            .initial = VK_IMAGE_LAYOUT_UNDEFINED,
            .final = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .clear = crd::core::make_clear({{ 1, 1, 1, 1 }}),
            .owning = true,
            .discard = false
        } },
        .subpasses = { {
            .attachments = { 0 },
            .preserve = {},
            .input = {}
        } },
        .dependencies = { {
            .source_subpass = crd::core::external_subpass,
            .dest_subpass = 0,
            .source_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dest_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .source_access = {},
            .dest_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
        } },
        .framebuffers = {
            { { 0 } }
        }
    });

    while (!crd::wm::is_closed(window)) {
        crd::wm::poll_events();
    }
    return 0;
}
