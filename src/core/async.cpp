#include <corundum/core/static_texture.hpp>
#include <corundum/core/static_model.hpp>
#include <corundum/core/static_mesh.hpp>
#include <corundum/core/async.hpp>

#include <utility>
#include <chrono>

namespace crd {
    template <typename T>
    crd_nodiscard crd_module T& Async<T>::get() noexcept {
        crd_unlikely_if(!result) {
            result = std::move(future.get());
        }
        return *result;
    }

    template <typename T>
    crd_nodiscard crd_module T& Async<T>::operator *() noexcept {
        return get();
    }

    template <typename T>
    crd_nodiscard crd_module T* Async<T>::operator ->() noexcept {
        return &get();
    }

    template <typename T>
    crd_nodiscard crd_module bool Async<T>::is_ready() noexcept {
        using namespace std::literals;
        crd_likely_if(result) {
            return true;
        }
        return future.wait_for(0ms) == std::future_status::ready;
    }

    template <typename T>
    crd_nodiscard crd_module bool Async<T>::valid() const noexcept {
        return future.valid() || result;
    }

    template struct Async<StaticMesh>;
    template struct Async<StaticTexture>;
    template struct Async<StaticModel>;
} // namespace crd
