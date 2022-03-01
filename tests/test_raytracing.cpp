#include <common.hpp>
#include <corundum/core/acceleration_structure.hpp>

int main() {
    auto window = crd::make_window(1280, 720, "Test RT");
    auto context = crd::make_context();
    auto renderer = crd::make_renderer(context);
    auto swapchain = crd::make_swapchain(context, window);
    std::vector<crd::Async<crd::StaticModel>> models;
    models.emplace_back(crd::request_static_model(context, "../data/models/cube/cube.obj"));
    models.emplace_back(crd::request_static_model(context, "../data/models/plane/plane.obj"));
    models.emplace_back(crd::request_static_model(context, "../data/models/sponza/sponza.obj"));

    while (!window.is_closed()) {
        crd::poll_events();
    }
}