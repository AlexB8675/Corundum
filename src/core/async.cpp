#include <corundum/core/static_mesh.hpp>
#include <corundum/core/async.hpp>

#include <utility>
#include <chrono>

namespace crd::core {
    template <typename T>
    crd_nodiscard crd_module T& Async<T>::get() noexcept {
        if (!result) {
            result = std::move(future.get());
        }
        return *result;
    }

    template <typename T>
    crd_nodiscard crd_module bool Async<T>::is_ready() noexcept {
        using namespace std::literals;
        if (result) {
            return true;
        }
        return future.wait_for(0ms) == std::future_status::ready;
    }

    template struct Async<StaticMesh>;
} // namespace crd::core
