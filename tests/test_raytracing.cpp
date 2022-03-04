#include <common.hpp>

struct CameraUniform {
    glm::mat4 projection;
    glm::mat4 view;
};

int main() {
    auto window = crd::make_window(1280, 720, "Test RT");
    auto context = crd::make_context();
    auto renderer = crd::make_renderer(context);
    auto swapchain = crd::make_swapchain(context, window);
    std::vector<crd::Async<crd::StaticModel>> models;
    models.emplace_back(crd::request_static_model(context, "../data/models/cube/cube.obj"));
    models.emplace_back(crd::request_static_model(context, "../data/models/plane/plane.obj"));

    auto result = crd::make_image(context, {
        .width   = window.width,
        .height  = window.height,
        .mips    = 1,
        .layers  = 1,
        .format  = VK_FORMAT_B8G8R8A8_UNORM,
        .aspect  = VK_IMAGE_ASPECT_COLOR_BIT,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .usage   = VK_IMAGE_USAGE_STORAGE_BIT |
                   VK_IMAGE_USAGE_TRANSFER_SRC_BIT
    });

    // Transition
    {
        auto commands = crd::make_command_buffer(context, {
            .pool = context.graphics->pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY
        });
        commands
            .begin()
            .transition_layout({
                .image = &result,
                .mip = 0,
                .level = 0,
                .source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                .dest_stage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                .source_access = {},
                .dest_access = VK_ACCESS_SHADER_WRITE_BIT,
                .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                .new_layout = VK_IMAGE_LAYOUT_GENERAL
            })
            .end();
        crd::immediate_submit(context, commands, crd::queue_type_graphics);
    }

    auto main_pipeline = crd::make_pipeline(context, renderer, {
        .raygen = "../data/shaders/test_raytracing/main.rgen.spv",
        .raymiss = "../data/shaders/test_raytracing/main.rmiss.spv",
        .raychit = "../data/shaders/test_raytracing/main.rchit.spv",
        .states = {}
    });

    auto camera_buffer = crd::make_buffer(context, {
        .type = crd::uniform_buffer,
        .usage = crd::host_visible,
        .capacity = sizeof(CameraUniform)
    });

    auto main_set = crd::make_descriptor_set(context, main_pipeline.layout.sets[0]);

    Camera camera;
    double last_time = 0;
    while (!window.is_closed()) {
        crd::poll_events();
        auto [commands, image, index, wait, signal, done] = crd::acquire_frame(context, renderer, window, swapchain);
        const auto current_time = crd::time();
        const auto delta_time = current_time - last_time;
        last_time = current_time;

        camera.update(window, delta_time);
        CameraUniform camera_data;
        camera_data.projection = camera.projection;
        camera_data.projection[1][1] *= -1;
        camera_data.projection = glm::inverse(camera_data.projection);
        camera_data.view = glm::inverse(camera.view);

        crd::wait_fence(context, done);

        camera_buffer.write(&camera_data, 0, sizeof camera_data);

        main_set[index]
            .bind(context, main_pipeline.bindings["Camera"], camera_buffer[index].info())
            .bind(context, main_pipeline.bindings["image"], result.info(VK_IMAGE_LAYOUT_GENERAL))
            .bind(context, main_pipeline.bindings["tlas"], std::vector{
                models[0]->submeshes[0].mesh->tlas.handle,
            });

        std::uint32_t tlas_index = 0;
        commands
            .begin()
            .bind_pipeline(main_pipeline)
            .bind_descriptor_set(0, main_set[index])
            .push_constants(VK_SHADER_STAGE_RAYGEN_BIT_KHR, &tlas_index, sizeof tlas_index)
            .trace_rays(window.width, window.height)
            .transition_layout({
                .image = &result,
                .mip = 0,
                .level = 0,
                .source_stage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                .dest_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .source_access = VK_ACCESS_SHADER_WRITE_BIT,
                .dest_access = VK_ACCESS_TRANSFER_READ_BIT,
                .old_layout = VK_IMAGE_LAYOUT_GENERAL,
                .new_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
            })
            .transition_layout({
                .image = &image,
                .mip = 0,
                .level = 0,
                .source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                .dest_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .source_access = {},
                .dest_access = VK_ACCESS_TRANSFER_WRITE_BIT,
                .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                .new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            })
            .copy_image(result, image)
            .transition_layout({
                .image = &image,
                .mip = 0,
                .level = 0,
                .source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .dest_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                .source_access = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dest_access = {},
                .old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .new_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
            })
            .transition_layout({
                .image = &result,
                .mip = 0,
                .level = 0,
                .source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .dest_stage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                .source_access = {},
                .dest_access = VK_ACCESS_SHADER_WRITE_BIT,
                .old_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .new_layout = VK_IMAGE_LAYOUT_GENERAL
            })
            .end();
        crd::present_frame(context, renderer, {
            .commands = commands,
            .window = window,
            .swapchain = swapchain,
            .waits = {},
            .stages = { VK_PIPELINE_STAGE_TRANSFER_BIT }
        });
    }
}