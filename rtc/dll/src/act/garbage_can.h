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

class garbage_can {
    struct alignas(16) Block {
        std::byte data[16];
    };

    struct TypeBin {
        std::vector<Block> storage;
        std::size_t count = 0;
        std::size_t stride = 0;
        std::function<void(std::byte*, std::size_t)> destroy;

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
                storage.reserve(std::max(2 * storage.capacity(), required_blocks));
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
		assert((this->bins.empty() || that.bins.empty()) && "I should not swap with a garbage_can with existing contents, consider using |=");
		if (this != &that) 
        {
		    bins = std::move(that.bins);
			assert(that.bins.empty());
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
        }

        return type_bin_handle<T>{ &bin };
    }

    template<typename T>
    void add(T&& obj) {
		get_type_bin<T>().add(std::forward<T>(obj));
    }

    RTC_CALL void clear();

    RTC_CALL void merge_from(garbage_can&& other);
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
