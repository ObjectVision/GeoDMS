// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_ACT_ANY_H)
#define __RTC_ACT_ANY_H

#include <any>
#include <concepts>
#include <memory>
#include <type_traits>

#include "dbg/Check.h"

//----------------------------------------------------------------------
// class template  : rtc::any::Any<T>
// 
// is almost equivalent with std::any<T>
// use this
// - when small object optimization is not needed due to sparse usage. This any only takes 8 bytes (nullptr) when not used
// - with move only objects
// 
// 
// equal expressions with rtc::any::Any a or std::any a;
// a.emplace<T>(args...)          
// rtc::any::Any<T>                   std::any<T>
// get reference to object:
// a.get<T>();                        auto pa = std::any_cast<T*>(a); if(!pa) throwIllegalAbstract(MG_POS, "Any::get"); return *pa; 
// rtc::any::make_any(obj);           std::any(obj)
// rtc::any::make_any<T>(args...);    std::any(std::in_place_type_t<T>, args...)
//----------------------------------------------------------------------
namespace rtc {
	// this class is almost equivalent with 
	namespace any {

		struct WrapBase {
			virtual ~WrapBase() {}
		};

		template <typename T>
		struct Wrapper : WrapBase {
			template <typename ...Args>
			Wrapper(Args&& ...args)
				: m_Value{ std::forward<Args>(args)... }
			{}

			T m_Value;
		};


		struct Any {
			std::unique_ptr<WrapBase> m_Wrapper;

			template <typename T, typename ...Args>
			void emplace(Args&& ...args)
			{
				m_Wrapper = std::make_unique<Wrapper<T>>(std::forward<Args>(args)...);
			}

//			explicit operator bool() const { return bool(m_Wrapper); }
			bool has_value() const { return m_Wrapper.operator bool(); }

			template <typename T>
			const T& Get() const
			{
				auto self = dynamic_cast<Wrapper<T>*>(m_Wrapper.get());
				if (!self)
					throwIllegalAbstract(MG_POS, "Any::get");
				return self->m_Value;
			}

			template <typename T>
			T& Get()
			{
				auto self = dynamic_cast<Wrapper<T>*>(m_Wrapper.get());
				if (!self)
					throwIllegalAbstract(MG_POS, "Any::get");
				return self->m_Value;
			}

			void Clear()
			{
				m_Wrapper.reset();
			}
		};

		template <typename T, typename ...Args>
		Any make_any(Args&& ...args)
		{
			Any result;
			result.emplace<T>(std::forward<Args>(args)...);
			return result;
		}

		template <typename T>
		Any make_any(T&& obj)
		{
			Any result;
			result.emplace<T>(std::forward<T>(obj));
			return result;
		}

		template <typename T> T any_cast(const Any& a) { return a.Get<T>(); }
		template <typename T> T any_cast(Any&& a) { return std::move(a.Get<T>()); }
		template <typename T> T* any_cast(      Any* a) { return &(a->Get<T>()); }
		template <typename T> const T* any_cast(const Any* a) { return &(a->Get<T>()); }
	} // namespace any
} // namespace rtc

template <typename T>
concept ClassType = std::is_class_v<T>;

template <ClassType T>
struct add_dummy_copy_constructor : T
{
    template <typename... Args>
    add_dummy_copy_constructor(Args&&... args)
        :	T(std::forward<Args>(args)...)
    {}

    add_dummy_copy_constructor(T&& org)
        : T(std::move(org))
    {}

	add_dummy_copy_constructor(add_dummy_copy_constructor<T>&&) = default;
	add_dummy_copy_constructor<T>& operator=(add_dummy_copy_constructor<T>&&) = default;
	//	~add_dummy_copy_constructor() override = default; // ensure that the destructor is virtual

	//	add_dummy_copy_constructor(add_dummy_copy_constructor const&) = delete;
	add_dummy_copy_constructor(const add_dummy_copy_constructor<T> &) { throwIllegalAbstract(MG_POS, "add_dummy_copy_constructor::copy CTOR"); }
	//	add_dummy_copy_constructor& operator=(add_dummy_copy_constructor const&) = delete;
	add_dummy_copy_constructor<T>& operator=(const add_dummy_copy_constructor<T> &) { throwIllegalAbstract(MG_POS, "add_dummy_copy_constructor::copy assignment"); }
};

template <class T, class... Args>
std::any make_noncopyable_any(Args&&...args)
{
	return std::make_any<add_dummy_copy_constructor<T>>(std::forward<Args>(args)...);
}

template <typename T> T noncopyable_any_cast(const std::any& a) { return *std::any_cast<add_dummy_copy_constructor<T>>(&a); }
template <typename T> T noncopyable_any_cast(std::any&& a) { return std::any_cast<add_dummy_copy_constructor<T>>(std::move(a)); }
template <typename T> T* noncopyable_any_cast(std::any* a) { return std::any_cast<add_dummy_copy_constructor<T>>(a); }
template <typename T> const T* noncopyable_any_cast(const std::any* a) { return std::any_cast<add_dummy_copy_constructor<T>>(a); }

// ---------------------------------------------------------------------- garbage_t
/*
using garbage_t = std::vector<std::any>;

template <typename Thing>
concept NotJustAnyGarbageDestructible = std::destructible<Thing> && !std::is_same_v<std::decay_t<Thing>, std::any> && !std::is_same_v<std::decay_t<Thing>, garbage_t>;

inline void operator |=(garbage_t& lhs, garbage_t&& rhs)
{
	if (lhs.empty())
		lhs.swap(rhs);
	else if (!rhs.empty())
	{
		lhs.insert(lhs.end(), std::make_move_iterator(rhs.begin()), std::make_move_iterator(rhs.end()));
		garbage_t().swap(rhs); // free emptied holders without calling any destructors of the std::any containees, as they have been relocated
	}
}

inline void operator |=(garbage_t& lhs, std::any&& rhs)
{
	lhs.emplace_back(std::move(rhs));
}

template <NotJustAnyGarbageDestructible Garbage>
void operator |=(garbage_t& oldGarbage, Garbage&& newGarbage) //requires !std::is_same_v<std::decay_t<Garbage>, std::any>
{
	oldGarbage |= make_noncopyable_any<Garbage>(std::move(newGarbage));
}

template <NotJustAnyGarbageDestructible Garbage>
garbage_t operator | (garbage_t&& oldGarbage, Garbage&& newGarbage) //requires !std::is_same_v<std::decay_t<Garbage>, std::any>
{
	oldGarbage |= std::forward<Garbage>(newGarbage);
	return std::move(oldGarbage);
}
*/

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
                for (std::size_t i = 0; i < count; ++i) {
                    T* ptr = reinterpret_cast<T*>(base + i * stride);
                    ptr->~T();
                }
                };
        }

        return type_bin_handle<T>{ &bin };
    }

    template<typename T>
    void add(T&& obj) {
		get_type_bin<T>().add(std::forward<T>(obj));
    }

    void clear() {
        for (auto& [_, bin] : bins) {
            if (bin.destroy) {
                bin.destroy(reinterpret_cast<std::byte*>(bin.storage.data()), bin.count);
            }
        }
        bins.clear();
    }

    void merge_from(garbage_can&& other) {
        for (auto& [tid, other_bin] : other.bins) {
            auto& bin = bins[tid];

            if (bin.storage.empty()) {
                bin = std::move(other_bin);
                continue;
            }

            assert(bin.stride == other_bin.stride);

            bin.ensure_capacity(bin.count + other_bin.count);

            std::byte* src = reinterpret_cast<std::byte*>(other_bin.storage.data());
            std::byte* dst = reinterpret_cast<std::byte*>(bin.storage.data());

            std::size_t dst_offset = bin.count * bin.stride;
            std::size_t src_bytes = other_bin.count * other_bin.stride;

            std::memmove(dst + dst_offset, src, src_bytes);

            bin.count += other_bin.count;
        }

        other.bins.clear();
    }
};


// Pipe-insertion operators for syntax sugar
template<typename T>
inline void operator|=(garbage_can& gc, T&& obj) {
    gc.add(std::move(obj));
}

// Merge operator
inline void operator|=(garbage_can& gc, garbage_can&& that) {
    gc.merge_from(std::move(that));
}

using garbage_t = garbage_can;

#endif //!defined(__RTC_ACT_ANY_H)
