// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_ACT_GARBAGE_CAN_H)
#define __RTC_ACT_GARBAGE_CAN_H

#include <vector>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <cstddef>
#include <cassert>
#include <functional>
#include <memory>
#include <new>
#include <cstring>
#include <algorithm>

// ---------------------------------------------------------------------- garbage_can
// Deferred destruction: objects are placement-constructed into per-type bins and destroyed at clear()
// or at the can's end of life. On growth and on merge_from the objects are MOVED (move constructor,
// then destructor of the source), so T must be move constructible and must not register its own
// address anywhere: a moved object lives at a new address.

class garbage_can {
    struct alignas(16) Block {
        std::byte data[16];
    };

    struct TypeBin {
        std::vector<Block> storage;
        std::size_t count = 0;
        std::size_t stride = 0;
        std::function<void(std::byte*, std::size_t)> destroy;
        // Moves count objects from src to dst by their move constructor and destroys the sources. The
        // objects are relocated on growth (ensure_capacity) and on merge_from; a bytewise move would
        // only be right for trivially relocatable types.
        std::function<void(std::byte* src, std::byte* dst, std::size_t count)> relocate;

        TypeBin() = default;
        TypeBin(TypeBin&&) noexcept = default;
        TypeBin& operator=(TypeBin&&) noexcept = default;

        TypeBin(const TypeBin&) = delete;
        TypeBin& operator=(const TypeBin&) = delete;

        void* get_slot(std::size_t i) {
            return static_cast<void*>(reinterpret_cast<std::byte*>(storage.data()) + i * stride);
        }

        void ensure_capacity(std::size_t min_count) {
            std::size_t required_bytes = min_count * stride;
            std::size_t required_blocks = (required_bytes + sizeof(Block) - 1) / sizeof(Block);
            if (storage.capacity() < required_blocks) {
                // Growing means a new buffer: the objects already added are moved into it by their own
                // move constructor, never bytewise as vector::reserve would do. Only a trivially
                // relocatable type survives a bytewise move; a std::list or an MSVC std::function would not.
                std::vector<Block> grown;
                grown.reserve(std::max(2 * storage.capacity(), required_blocks));
                grown.resize(required_blocks);
                if (count && relocate)
                    relocate(reinterpret_cast<std::byte*>(storage.data()), reinterpret_cast<std::byte*>(grown.data()), count);
                storage.swap(grown);
                return;
            }
            if (storage.size() < required_blocks) {
                storage.resize(required_blocks);
            }
        }
    };

    std::unordered_map<std::type_index, TypeBin> bins;

public:

    template<typename T>
    struct type_bin_handle {
        TypeBin* bin;

        void add(T&& obj) {
            assert(bin);
            bin->ensure_capacity(bin->count + 1);
            void* dest = bin->get_slot(bin->count);
            new (dest) T(std::move(obj));
            ++bin->count;
        }
    };


    garbage_can() = default;
    ~garbage_can() { clear(); }

    garbage_can(const garbage_can&) = delete;
    garbage_can& operator=(const garbage_can&) = delete;

    garbage_can(garbage_can&&) noexcept = default;
    garbage_can& operator=(garbage_can&& that) noexcept
	{
		if (this != &that)
		{
			clear(); // run what this can still holds; replacing the bins outright would drop those deferred destructions
			bins = std::move(that.bins);
			that.bins.clear(); // a moved-from unordered_map is valid but unspecified; make the source verifiably empty
		}
		return *this;
	}

    template<typename T>
    type_bin_handle<T> get_type_bin() {
        static_assert(std::is_move_constructible_v<T>);
        static_assert(std::is_destructible_v<T>);
        static_assert(alignof(T) <= 16, "T exceeds max supported alignment");

        const std::type_index tid(typeid(T));
        auto& bin = bins[tid];

        if (bin.storage.empty()) {
            constexpr std::size_t stride = std::max(sizeof(T), alignof(T));
            bin.stride = stride;
            bin.destroy = [](std::byte* base, std::size_t count) {
                constexpr std::size_t stride = std::max(sizeof(T), alignof(T));

                while (count--) 
                {
                    T* ptr = reinterpret_cast<T*>(base);
                    ptr->~T();
					base += stride;
                }
               };
            bin.relocate = [](std::byte* src, std::byte* dst, std::size_t count) {
                constexpr std::size_t stride = std::max(sizeof(T), alignof(T));

                while (count--)
                {
                    T* obj = reinterpret_cast<T*>(src);
                    new (dst) T(std::move(*obj));
                    obj->~T();
                    src += stride;
                    dst += stride;
                }
            };
        }

        return type_bin_handle<T>{ &bin };
    }

    template<typename T>
    void add(T&& obj) {
		get_type_bin<T>().add(std::forward<T>(obj));
    }

    RTC_CALL void clear();

    void merge_from(garbage_can&& other);
};


// Pipe-insertion operators for syntax sugar
template<std::move_constructible T>
inline void operator|=(garbage_can& gc, T&& obj) {
    gc.add(std::move(obj));
}

// Merge operator
inline void operator|=(garbage_can& gc, garbage_can&& that) {
    gc.merge_from(std::move(that));
}


#endif //!defined(__RTC_ACT_GARBAGE_CAN_H)
