//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>

#pragma once

#if !defined(__RTC_ACT_ANY_H)
#define __RTC_ACT_ANY_H

#include <any>
#include <memory>
#include <type_traits>

#include "dbg/Check.h"

//----------------------------------------------------------------------
// class template  : rtc::any::Any<T>
// 
// is almost equivalent with std::any<T>
// so TODO: consider replacing with std version
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
			explicit operator bool() const { return bool(m_Wrapper); }

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

using garbage_t = std::vector<std::any>;

inline void operator |=(garbage_t& lhs, garbage_t&& rhs)
{
	if (lhs.empty())
		lhs = std::move(rhs);
	else if (!rhs.empty())
	{
		lhs.insert(lhs.end(), std::make_move_iterator(rhs.begin()), std::make_move_iterator(rhs.end()));
		rhs = {}; // free emptied holders/
	}
}

inline void operator |=(garbage_t& lhs, std::any&& rhs)
{
	lhs.emplace_back(std::move(rhs));
}

#include <concepts>
template <typename Thing>
concept NotJustAnyDestructible = std::destructible<Thing> && !std::is_same_v<std::decay_t<Thing>, std::any>;

template <typename Thing>
concept NotJustGarbageDestructible = std::destructible<Thing> && !std::is_same_v<std::decay_t<Thing>, garbage_t>;

template <typename Thing>
concept NotJustAnyGarbageDestructible = NotJustAnyDestructible<Thing> && NotJustGarbageDestructible<Thing>;

template <typename T>
struct add_dummy_copy_constructor : T 
{

	template <typename... Args>
	add_dummy_copy_constructor(Args&&... args) 
		:	T(std::forward<Args>(args)...) 
	{}

	add_dummy_copy_constructor(T&& org)
		: T(std::move(org))
	{}

	add_dummy_copy_constructor(add_dummy_copy_constructor const&) 
	{
		throwIllegalAbstract(MG_POS, "add_dummy_copy_constructor");
	}

	add_dummy_copy_constructor(add_dummy_copy_constructor&&) = default;
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


#endif //!defined(__RTC_ACT_ANY_H)
