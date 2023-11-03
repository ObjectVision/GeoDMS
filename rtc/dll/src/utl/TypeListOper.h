// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__RTC_UTL_TYPELISTOPER_H)
#define __RTC_UTL_TYPELISTOPER_H


#include "cpc/transform.h"

#include <boost/mpl/placeholders.hpp>

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
				cpair(param_type_t<Args>... args) : m_First(args...), m_Second(args...) {}
				T m_First;
				U m_Second;
			};
		};
		template <template <typename T> typename F, typename... Args>
		struct constructed_pair_templ_functor {
			template <typename T, typename U>
			struct cpair
			{
				cpair(param_type_t<Args>... args) : m_First(args...), m_Second(args...) {}
				F<T> m_First;
				U m_Second;
			};
		};
	}

	// =================== using boost::mpl 

	template<typename TL, typename... Args>
	struct tuple_func
	{
		using type = typename boost::mpl::fold< TL
			, impl::empty_base<Args...>
			, typename impl::constructed_pair_functor<Args...>::template cpair<boost::mpl::_2, boost::mpl::_1>
		>::type;
	};

	template<typename TL, typename F, typename... Args>
	using inst_tuple = typename tuple_func<tl::transform<TL, F>, Args... >::type;

	// =================== end using boost::mpl 

	template<typename TL, template <typename T> typename F, typename... Args>
	struct tuple_templ_func
	{
		using type = typename boost::mpl::fold< TL
			, impl::empty_base<Args...>
			, typename impl::constructed_pair_templ_functor<F, Args...>::template cpair<boost::mpl::_2, boost::mpl::_1>
		>::type;
	};

	template<typename TL, template <typename T> typename F, typename... Args>
	using inst_tuple_templ = typename tuple_templ_func<TL, F, Args...>::type;

	
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
		&T::GetDynamicClass; // take address to instantiate
	}
};

template <typename TypeList>
struct TypeListClassReg {
	TypeListClassReg() {
		boost::mpl::for_each<TypeList, wrap<boost::mpl::placeholders::_> >(ClassReg());
	}
};


#endif // __RTC_UTL_TYPELISTOPER_H
