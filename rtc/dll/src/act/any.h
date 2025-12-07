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
			bool is_a() const
			{
				auto self = dynamic_cast<Wrapper<T>*>(m_Wrapper.get());
				return self != nullptr;
			}

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


#endif //!defined(__RTC_ACT_ANY_H)
