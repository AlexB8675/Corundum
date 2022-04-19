#include <corundum/core/acceleration_structure.hpp>
#include <corundum/core/static_model.hpp>
#include <corundum/core/static_model.hpp>
#include <corundum/core/static_mesh.hpp>
#include <corundum/core/async.hpp>

#include <Tracy.hpp>

#include <utility>
#include <chrono>

namespace crd {
    template <typename T>
    static inline void destroy_async(Async<T>* async) noexcept {
        crd_profile_scoped();
        switch (async->tag) {
            case task_tag_running: {
                auto task = async->future();
                crd_unlikely_if(task->valid()) {
                    task->get();
                }
                task->~future();
            } break;

            case task_tag_completed: {
                async->object()->~T();
            } break;
        }
    }

    template <typename T>
    crd_nodiscard crd_module Async<T> make_async(std::future<T>&& task) noexcept {
        crd_profile_scoped();
        Async<T> async;
        new (&async.storage) std::future(std::move(task));
        async.tag = task_tag_running;
        return async;
    }

    template <typename T>
    Async<T>::~Async() noexcept {
        crd_profile_scoped();
        destroy_async(this);
    }

    template <typename T>
    Async<T>::Async(Async&& other) noexcept {
        crd_profile_scoped();
        *this = std::move(other);
    }

    template <typename T>
    Async<T>& Async<T>::operator =(Async&& other) noexcept {
        crd_profile_scoped();
        destroy_async(this);
        tag = other.tag;
        switch (tag) {
            case task_tag_running: {
                new (&storage) std::future(std::move(*other.future()));
            } break;

            case task_tag_completed: {
                new (&storage) T(std::move(*other.object()));
            } break;
        }
        other.tag = task_tag_none;
        return *this;
    }

    template <typename T>
    crd_nodiscard crd_module T& Async<T>::get() noexcept {
        crd_profile_scoped();
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
        crd_profile_scoped();
        return get();
    }

    template <typename T>
    crd_nodiscard crd_module T* Async<T>::operator ->() noexcept {
        crd_profile_scoped();
        return &get();
    }

    template <typename T>
    crd_nodiscard crd_module bool Async<T>::is_ready() noexcept {
        crd_profile_scoped();
        using namespace std::literals;
        return tag == task_tag_completed || future()->wait_for(0ms) == std::future_status::ready;
    }

    template <typename T>
    crd_nodiscard std::future<T>* Async<T>::future() noexcept {
        crd_profile_scoped();
        return std::launder(reinterpret_cast<std::future<T>*>(&storage));
    }

    template <typename T>
    crd_nodiscard T* Async<T>::object() noexcept {
        crd_profile_scoped();
        return std::launder(reinterpret_cast<T*>(&storage));
    }

    template struct Async<StaticMesh>;
    template struct Async<StaticTexture>;
    template struct Async<StaticModel>;

    template crd_module Async<StaticMesh> make_async(std::future<StaticMesh>&&);
    template crd_module Async<StaticTexture> make_async(std::future<StaticTexture>&&);
    template crd_module Async<StaticModel> make_async(std::future<StaticModel>&&);
} // namespace crd
