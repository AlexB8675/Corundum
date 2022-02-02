#include <corundum/core/static_texture.hpp>
#include <corundum/core/static_model.hpp>
#include <corundum/core/static_mesh.hpp>
#include <corundum/core/async.hpp>

#include <utility>
#include <chrono>

namespace crd {
    template <typename T>
    Async<T>::~Async() noexcept {
        switch (tag) {
            case task_tag_running:   future()->~future(); break;
            case task_tag_completed: object()->~T();      break;
        }
    }

    template <typename T>
    Async<T>::Async(Async&& other) noexcept {
        *this = std::move(other);
    }

    template <typename T>
    Async<T>& Async<T>::operator =(Async&& other) noexcept {
        tag = other.tag;
        switch (tag) {
            case task_tag_running: {
                new (&storage) std::future<T>(std::move(*other.future()));
            } break;

            case task_tag_completed: {
                new (&storage) T(std::move(*other.object()));
            } break;
        }
        return *this;
    }

    template <typename T>
    crd_module void Async<T>::import(std::future<T>&& future) noexcept {
        crd_assert(tag == task_tag_none, "reuse of Async<T> not permitted");
        new (&storage) std::future<T>(std::move(future));
        tag = task_tag_running;
    }

    template <typename T>
    crd_nodiscard crd_module T& Async<T>::get() noexcept {
        crd_unlikely_if(tag == task_tag_running) {
            auto task = future();
            auto result = task->get();
            task->~future();
            tag = task_tag_completed;
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
        return tag == task_tag_completed || future()->wait_for(0ms) == std::future_status::ready;
    }

    template <typename T>
    crd_nodiscard std::future<T>* Async<T>::future() noexcept {
        return std::launder(reinterpret_cast<std::future<T>*>(&storage));
    }

    template <typename T>
    crd_nodiscard T* Async<T>::object() noexcept {
        return std::launder(reinterpret_cast<T*>(&storage));
    }

    template struct Async<StaticMesh>;
    template struct Async<StaticTexture>;
    template struct Async<StaticModel>;
} // namespace crd
