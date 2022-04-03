#pragma once

#include <corundum/detail/macros.hpp>

#include <type_traits>
#include <string_view>
#include <string>
#include <new>

namespace crd {
    namespace dtl {
        crd_noreturn void _panic(const char* msg) {
            std::printf("crd::_panic() was called with: %s", msg);
            std::terminate();
        }
    } // namespace crd::dtl

    template <typename E>
    crd_nodiscard constexpr const char* stringify(E) noexcept;

    template <typename T, typename E>
    struct Expected {
        static_assert(std::is_enum_v<E>, "E is not an enum type");
        std::aligned_union_t<0, T, E> storage;
        enum Tag {
            tag_value,
            tag_error
        } tag;

        constexpr Expected() noexcept;
        constexpr Expected(T&&) noexcept;
        constexpr Expected(E) noexcept;
        template <typename... Args>
        constexpr Expected(Args&&...) noexcept;


        constexpr Expected(const Expected&) noexcept;
        constexpr Expected(Expected&&) noexcept;
        constexpr Expected& operator =(const Expected&) noexcept;
        constexpr Expected& operator =(Expected&&) noexcept;

        crd_nodiscard constexpr T value() noexcept;
        crd_nodiscard constexpr T unwrap() noexcept;
        crd_nodiscard constexpr E error() noexcept;

        crd_nodiscard constexpr bool is_value() const noexcept;
        crd_nodiscard constexpr bool is_error() const noexcept;

        crd_nodiscard constexpr explicit operator bool() const noexcept;
        crd_nodiscard constexpr bool operator !() const noexcept;
    };

    template <typename T, typename E>
    crd_nodiscard constexpr Expected<T, E>::Expected() noexcept {
        new (&storage) T();
        tag = Expected<T, E>::tag_value;
    }

    template <typename T, typename E>
    crd_nodiscard constexpr Expected<T, E>::Expected(T&& value) noexcept {
        new (&storage) T(std::forward<T>(value));
        tag = Expected<T, E>::tag_value;
    }

    template <typename T, typename E>
    crd_nodiscard constexpr Expected<T, E>::Expected(E error) noexcept {
        new (&storage) E{ error };
        tag = Expected<T, E>::tag_error;
    }

    template <typename T, typename E>
    template <typename... Args>
    crd_nodiscard constexpr Expected<T, E>::Expected(Args&&... args) noexcept {
        if constexpr (std::is_aggregate_v<T>) {
            new (&storage) T{ std::forward<Args>(args)... };
        } else {
            new (&storage) T(std::forward<Args>(args)...);
        }
        tag = Expected<T, E>::tag_value;
    }

    template <typename T, typename E>
    constexpr Expected<T, E>::Expected(const Expected& other) noexcept {
        *this = other;
    }

    template <typename T, typename E>
    constexpr Expected<T, E>& Expected<T, E>::operator =(const Expected& other) noexcept {
        if constexpr (!std::is_trivial_v<T>) {
            if (tag == tag_value) {
                value().~T();
            }
        }
        switch (tag = other.tag) {
            case tag_value: new (&storage) T{ other.value() }; break;
            case tag_error: new (&storage) E{ other.error() }; break;
        }
        return *this;
    }

    template <typename T, typename E>
    constexpr Expected<T, E>::Expected(Expected&& other) noexcept {
        *this = std::move(other);
    }

    template <typename T, typename E>
    constexpr Expected<T, E>& Expected<T, E>::operator =(Expected&& other) noexcept {
        if constexpr (!std::is_trivial_v<T>) {
            if (tag == tag_value) {
                value().~T();
            }
        }
        switch (tag = other.tag) {
            case tag_value: new (&storage) T{ std::move(other.value()) }; break;
            case tag_error: new (&storage) E{ other.error() }; break;
        }
        return *this;
    }

    template <typename T, typename E>
    crd_nodiscard constexpr T Expected<T, E>::value() noexcept {
        crd_unlikely_if(tag != tag_value) {
            const auto err_string = std::string("called value() on error Expected<T, E> with message: ") + stringify(error());
            dtl::_panic(err_string.c_str());
        }
        return *std::launder(reinterpret_cast<T*>(&storage));
    }

    template <typename T, typename E>
    crd_nodiscard constexpr T Expected<T, E>::unwrap() noexcept {
        return value();
    }

    template <typename T, typename E>
    crd_nodiscard constexpr E Expected<T, E>::error() noexcept {
        crd_unlikely_if(tag != tag_error) {
            dtl::_panic("called error() on value Expected<T, E>");
        }
        return *std::launder(reinterpret_cast<E*>(&storage));
    }

    template <typename T, typename E>
    crd_nodiscard constexpr bool Expected<T, E>::is_value() const noexcept {
        return tag == tag_value;
    }

    template <typename T, typename E>
    crd_nodiscard constexpr bool Expected<T, E>::is_error() const noexcept {
        return tag != tag_error;
    }

    template <typename T, typename E>
    crd_nodiscard constexpr Expected<T, E>::operator bool() const noexcept {
        return is_value();
    }

    template <typename T, typename E>
    crd_nodiscard constexpr bool Expected<T, E>::operator !() const noexcept {
        return is_error();
    }
} // namespace crd
