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

#if !defined(__RTC_UTL_TYPELISTOPER_H)
#define __RTC_UTL_TYPELISTOPER_H


#include "cpc/transform.h"

#include <boost/mpl/placeholders.hpp>
using namespace boost::mpl::placeholders;

//----------------------------------------------------------------------
// typelist operations
//----------------------------------------------------------------------

#include <boost/mpl/fold.hpp>

namespace tl_oper
{
	namespace impl {

		template <typename ...Args> struct empty_base
		{ 
			empty_base(typename param_type<Args>::type...) 
			{} 
		};

		template <typename Head, typename Tail>
		struct scattered_hierarchy
			:	Head
			,	Tail
		{};

		template <typename... Args>
		struct constructed_pair_functor {
			template <typename T, typename U>
			struct cpair
			{
				cpair(typename param_type<Args>::type... args) : m_First(args...), m_Second(args...) {}
				T m_First;
				U m_Second;
			};
		};
	}
/* REMOVE
	template<typename TL, typename... Args>
	struct tuple
		: boost::mpl::fold< TL
			, impl::empty_base<Args...>
			, typename impl::constructed_pair_functor<Args...>::template cpair<boost::mpl::_2, boost::mpl::_1>
		>
	{
		using typename fold::type;

		tuple(Args...args)
			: m_TupleData(std::move(args)...)
		{}

		type m_TupleData;
	};
	*/

	template<typename TL, typename... Args>
	struct tuple_func
	{
		using type = typename boost::mpl::fold< TL
			, impl::empty_base<Args...>
			, typename impl::constructed_pair_functor<Args...>::template cpair<boost::mpl::_2, boost::mpl::_1>
		>::type;
	};

	template<typename TL, typename F, typename... Args>
	struct inst_tuple
	{
		inst_tuple(Args...args)
			: m_InstTupleData(args...)
		{}

		typename tuple_func<tl::transform<TL, F>, Args... >::type m_InstTupleData;
	};

}	// namespace tl_oper

//----------------------------------------------------------------------
// MPL defs for Registering Classs RTTI
//----------------------------------------------------------------------

#include <boost/mpl/for_each.hpp>

template <class T> struct wrap {};

struct ClassReg {
	template <class T> // T must be derived from Object (or at least have GetStaticClass and GetDynamicClass defined)
	void operator ()(wrap<T>) const // deduce T
	{
		T::GetStaticClass();   // Call to register
		&(T::GetDynamicClass); // take address to instantiate
	}
};

template <typename TypeList>
struct TypeListClassReg {
	TypeListClassReg() {
		boost::mpl::for_each<TypeList, wrap<_> >(ClassReg());
	}
};


#endif // __RTC_UTL_TYPELISTOPER_H
