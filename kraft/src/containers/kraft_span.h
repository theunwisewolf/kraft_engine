#pragma once

#include <initializer_list>

#include "core/kraft_core.h"

template <typename T> struct Span {
    using V = std::remove_cv_t<T>;

    T* ptr;
    u64 size;

    // Empty span
    constexpr Span() noexcept : ptr(nullptr), size(0) {}

    // From pointer and size
    constexpr Span(T* ptr, u64 size) noexcept : ptr(ptr), size(size) {}

    // From a C-style array
    template <u64 N> constexpr Span(T (&arr)[N]) noexcept : ptr(arr), size(N) {}

    // From an initializer_list
    // NOTE: The list is temporary and only safe when used within the same expression
    constexpr Span(std::initializer_list<V> list)
        requires std::is_const_v<T>
        : ptr(list.begin()),
          size(list.size()) {}

    constexpr T& operator[](u64 i) noexcept {
        return ptr[i];
    }

    constexpr const T& operator[](u64 i) const noexcept {
        return ptr[i];
    }

    constexpr const T& Front() noexcept {
        return ptr[0];
    }

    constexpr const T& Back() noexcept {
        return ptr[size - 1];
    }

    constexpr const T* Data() noexcept {
        return ptr;
    }

    [[nodiscard]] constexpr u64 Size() const noexcept {
        return size;
    }

    [[nodiscard]] constexpr bool Empty() const noexcept {
        return size == 0;
    }

    [[nodiscard]] constexpr Span Subspan(u64 offset, u64 count) const {
        KASSERTM(offset <= size && count <= size - offset, "Span::Subspan: out of range");

        return Span(ptr + offset, count);
    }

    constexpr T* Begin() noexcept {
        return ptr;
    }

    constexpr T* End() noexcept {
        return ptr + size;
    }

    constexpr const T* Begin() const noexcept {
        return ptr;
    }

    constexpr const T* End() const noexcept {
        return ptr + size;
    }

    // For range-based C++ crap
    constexpr T* begin() noexcept {
        return ptr;
    }

    constexpr T* end() noexcept {
        return ptr + size;
    }

    constexpr const T* begin() const noexcept {
        return ptr;
    }

    constexpr const T* end() const noexcept {
        return ptr + size;
    }
};