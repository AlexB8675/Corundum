#include <corundum/core/static_texture.hpp>
#include <corundum/core/static_model.hpp>
#include <corundum/core/static_mesh.hpp>
#include <corundum/core/async.hpp>

#include <utility>
#include <chrono>

namespace crd {
    template <typename T>
    Async<T>::~Async() noexcept {
        crd_likely_if(tag == TaskTag::completed) {
            object()->~T();
        }
    }

    template <typename T>
    crd_module void Async<T>::import(std::future<T>&& future) noexcept {
        crd_assert(tag == TaskTag::none, "reuse of Async<T> not permitted");
        new (&storage) std::future<T>(std::move(future));
        tag = TaskTag::running;
    }

    template <typename T>
    crd_nodiscard crd_module T& Async<T>::get() noexcept {
        crd_unlikely_if(tag == TaskTag::running) {
            auto task = future();
            auto result = task->get();
            task->~future();
            tag = TaskTag::completed;
            new (&storage) T(std::move(result));
        }
        return *object();
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
        return tag == TaskTag::completed || future()->wait_for(0ms) == std::future_status::ready;
    }

    template <typename T>
    crd_nodiscard std::future<T>* Async<T>::future() noexcept {
        return static_cast<std::future<T>*>(static_cast<void*>(&storage));
    }

    template <typename T>
    crd_nodiscard T* Async<T>::object() noexcept {
        return static_cast<T*>(static_cast<void*>(&storage));
    }

    template struct Async<StaticMesh>;
    template struct Async<StaticTexture>;
    template struct Async<StaticModel>;
} // namespace crd
