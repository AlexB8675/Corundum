#include <corundum/core/swapchain.hpp>
#include <corundum/core/context.hpp>

#include <corundum/wm/window.hpp>

int main() {
    auto window    = crd::wm::make_window(1280, 720, "Sorting Algos");
    auto context   = crd::core::make_context();
    auto swapchain = crd::core::make_swapchain(context, window);

    while (!crd::wm::is_closed(window)) {
        crd::wm::poll_events();
    }
    return 0;
}
