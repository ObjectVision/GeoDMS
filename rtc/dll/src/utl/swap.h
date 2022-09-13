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

#ifndef __RTC_UTL_SWAP_H
#define __RTC_UTL_SWAP_H

#include "cpc/Types.h"

#include <algorithm> // contains std::swap
#include <boost/mpl/assert.hpp>

namespace omni {
	namespace impl {
		template <class T, T val> struct member_wrapper{};

		template <class T> char   test_for_swap(member_wrapper<void (T::*)(T&), &T::swap>* p = 0);
		template <class T> double test_for_swap(...);

		template <class T>
		struct has_member_swap
		{
			static T t;
			BOOST_STATIC_CONSTANT(bool,  value = (sizeof(test_for_swap<T>(0) ) == sizeof(char) ) );
		};

		template<bool b>
		struct swapper
		{
			template <class T>
			static void swap(T& t1, T& t2)
			{ 
				std::swap(t1, t2);
			}
		};

		template<>
		struct swapper<true>
		{
			template <class T>
			static void swap(T& t1, T& t2)
			{ 
				t1.swap(t2);
			};
		};

#if defined(MG_DEBUG_SWAP)
		struct CompilerCheckClass1 
		{ 
			void swap(CompilerCheckClass1& oth);
		};
		struct CompilerCheckClass2 {};
		struct CompilerCheckClass3 : CompilerCheckClass1 {};

		BOOST_MPL_ASSERT    (has_member_swap<CompilerCheckClass1>);
		BOOST_MPL_ASSERT_NOT(has_member_swap<CompilerCheckClass2>);
		BOOST_MPL_ASSERT_NOT(has_member_swap<CompilerCheckClass3>);
		BOOST_MPL_ASSERT_NOT(has_member_swap<int>);
#endif

	}	//	namespace impl

	template <class T>
	void swap(T& t1, T& t2)
	{
		impl::swapper<impl::has_member_swap<T>::value>::swap(t1, t2);
	}

}	//	namespace omni

#endif // __RTC_UTL_SWAP_H
