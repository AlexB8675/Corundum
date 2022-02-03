#pragma once

#include <corundum/detail/macros.hpp>

#include <type_traits>
#include <string>
#include <new>

namespace crd {
    crd_noreturn void panic(const char*);

    template <typename E>
    struct Error {
        E value;
    };

    template <typename E>
    crd_nodiscard constexpr const char* stringify_error(Error<E>) noexcept;
    template <typename E>
    crd_nodiscard constexpr E status_error(Error<E>) noexcept;

    template <typename T, typename E>
    struct Expected {
        static_assert(std::is_enum_v<E>, "E in Expected<T, E> is not an enum.");
        std::aligned_union_t<0, T, Error<E>> storage;
        enum Tag {
            tag_value,
            tag_error
        } tag;

        constexpr Expected() noexcept = default;
        constexpr Expected(const Expected&) noexcept;
        constexpr Expected(Expected&&) noexcept;
        constexpr Expected& operator =(const Expected&) noexcept;
        constexpr Expected& operator =(Expected&&) noexcept;

        crd_nodiscard                 T&       value() & noexcept;
        crd_nodiscard           const T&       value() const& noexcept;
        crd_nodiscard                 T&&      value() && noexcept;
        crd_nodiscard constexpr       Error<E> error() const noexcept;
        crd_nodiscard                 T&       unwrap() & noexcept;
        crd_nodiscard           const T&       unwrap() const& noexcept;
        crd_nodiscard                 T&&      unwrap() && noexcept;

        crd_nodiscard constexpr explicit operator bool() const noexcept;
        crd_nodiscard constexpr bool operator !() const noexcept;
    };

    template <typename T, typename E>
    crd_nodiscard constexpr Expected<T, E> make_expected() noexcept {
        Expected<T, E> expected;
        expected.tag = Expected<T, E>::tag_valie;
        new (&expected.storage) T();
        return expected;
    }

    template <typename T, typename E, typename... Args>
    crd_nodiscard constexpr Expected<T, E> make_expected(Args&&... args) noexcept {
        Expected<T, E> expected;
        if constexpr (std::is_aggregate_v<T>) {
            new (&expected.storage) T{ std::forward<Args>(args)... };
        } else {
            new (&expected.storage) T(std::forward<Args>(args)...);
        }
        expected.tag = Expected<T, E>::tag_value;
        return expected;
    }

    template <typename T, typename E>
    crd_nodiscard constexpr Expected<T, E> make_expected(T&& value) noexcept {
        Expected<T, E> expected;
        new (&expected.storage) T(std::forward<T>(value));
        expected.tag = Expected<T, E>::tag_value;
        return expected;
    }

    template <typename T, typename E>
    crd_nodiscard constexpr Expected<T, E> make_expected(E error) noexcept {
        Expected<T, E> expected;
        new (&expected.storage) Error<E>{ error };
        expected.tag = Expected<T, E>::tag_error;
        return expected;
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
            case tag_value: new (&storage) T(other.value()); break;
            case tag_error: new (&storage) Error<E>(other.error()); break;
        }
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
            case tag_value: new (&storage) T(std::move(other.value())); break;
            case tag_error: new (&storage) Error<E>(other.error()); break;
        }
    }

    template <typename T, typename E>
    crd_nodiscard T& Expected<T, E>::value() & noexcept {
        crd_unlikely_if(tag != tag_value) {
            std::string err_string =
                "called value() on error Expected<T, E>: " +
                stringify_error(error());
            panic(err_string.c_str());
        }
        return *std::launder(reinterpret_cast<T*>(&storage));
    }

    template <typename T, typename E>
    crd_nodiscard const T& Expected<T, E>::value() const& noexcept {
        crd_unlikely_if(tag != tag_value) {
            std::string err_string =
                "called value() on error Expected<T, E>: " +
                stringify_error(error());
            panic(err_string.c_str());
        }
        return *std::launder(reinterpret_cast<const T*>(&storage));
    }

    template <typename T, typename E>
    crd_nodiscard T&& Expected<T, E>::value() && noexcept {
        crd_unlikely_if(tag != tag_value) {
            std::string err_string =
                "called value() on error Expected<T, E>: " +
                stringify_error(error());
            panic(err_string.c_str());
        }
        return std::move(*std::launder(reinterpret_cast<T*>(&storage)));
    }

    template <typename T, typename E>
    crd_nodiscard constexpr Error<E> Expected<T, E>::error() const noexcept {
        crd_unlikely_if(tag != tag_value) {
            panic("called error() on value Expected<T, E>: ");
        }
        return *std::launder(reinterpret_cast<Error<E>*>(&storage));
    }

    template <typename T, typename E>
    crd_nodiscard T& Expected<T, E>::unwrap() & noexcept {
        return value();
    }

    template <typename T, typename E>
    crd_nodiscard const T& Expected<T, E>::unwrap() const& noexcept {
        return value();
    }

    template <typename T, typename E>
    crd_nodiscard T&& Expected<T, E>::unwrap() && noexcept {
        return value();
    }

    template <typename T, typename E>
    crd_nodiscard constexpr Expected<T, E>::operator bool() const noexcept {
        return tag == tag_value;
    }

    template <typename T, typename E>
    crd_nodiscard constexpr bool Expected<T, E>::operator !() const noexcept {
        return !bool(*this);
    }
} // namespace crd
