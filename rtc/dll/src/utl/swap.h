// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __RTC_UTL_SWAP_H
#define __RTC_UTL_SWAP_H

#include "cpc/Types.h"

#include <algorithm> // contains std::swap

namespace omni {
	namespace impl {
		template <class T, T val> struct member_wrapper{};

		template <class T> char   test_for_swap(member_wrapper<void (T::*)(T&), &T::swap>* p = 0);
		template <class T> double test_for_swap(...);

		template <class T>
		struct has_member_swap
		{
			static T t;
			static const bool value = sizeof(test_for_swap<T>(0) ) == sizeof(char);
		};

#if defined(MG_DEBUG_SWAP)
		struct CompilerCheckClass1 
		{ 
			void swap(CompilerCheckClass1& oth);
		};
		struct CompilerCheckClass2 {};
		struct CompilerCheckClass3 : CompilerCheckClass1 {};

		static_assert(has_member_swap<CompilerCheckClass1>::value);
		static_assert(has_member_swap<CompilerCheckClass2>::value == 0);
		static_assert(has_member_swap<CompilerCheckClass3>::value == 0);
		static_assert(has_member_swap<int>::value == 0);
#endif

	}	//	namespace impl

	template <class T>
	void swap(T& t1, T& t2) noexcept
	{
		if constexpr (impl::has_member_swap<T>::value)
			t1.swap(t2);
		else
			std::swap(t1, t2);
	}

}	//	namespace omni

#endif // __RTC_UTL_SWAP_H
