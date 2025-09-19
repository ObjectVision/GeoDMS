// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_UTL_TYPELISTOPER_H)
#define __RTC_UTL_TYPELISTOPER_H


#include "cpc/transform.h"

//----------------------------------------------------------------------
// typelist operations
//----------------------------------------------------------------------

namespace tl_oper
{
	// --------------------  runtime construction helpers  --------------------
	// Construct a tuple whose element types come from `List`, passing the SAME Args...
	// to each element's constructor.

	template<class... Ts>
	struct constructed_tuple_impl;

	template<class Head>
	struct constructed_tuple_impl<Head>
	{
		template<class... Args>
		explicit constructed_tuple_impl(Args&&... args)
			: head(std::forward<Args>(args)...)
		{}

		Head head;
	};

	template<class Head, class... Tail>
	struct constructed_tuple_impl<Head, Tail...> : constructed_tuple_impl<Tail...>
	{
		using Base = constructed_tuple_impl<Tail...>;

		template<class... Args>
		explicit constructed_tuple_impl(Args&&... args)
			: Base(std::forward<Args>(args)...)
			, head(std::forward<Args>(args)...)
		{}

		Head head;
	};

	template<class TL>
	struct constructed_tuple;

	template<class... Ts>
	struct constructed_tuple<tl::type_list<Ts...>> : constructed_tuple_impl<Ts...>
	{
		using Base = constructed_tuple_impl<Ts...>;

		template<class... Args>
		explicit constructed_tuple(Args&&... args)
			: Base(std::forward<Args>(args)...)
		{}
	};


	template<typename TL, typename FunctorF>
	struct inst_tuple
	{
		using TL2 = tl::transform_t<TL, FunctorF>;
		using tuple_type = constructed_tuple<TL2>;

		tuple_type m_ConstructedData;

		template<class... Args>
		inst_tuple(Args&&... args) : m_ConstructedData(std::forward<Args>(args)...) {}
	};

	template<typename TL, template <typename T> typename F>
	struct inst_tuple_templ : inst_tuple<TL, tl::bind_placeholders<F, ph::_1>>
	{
		using inst_tuple<TL, tl::bind_placeholders<F, ph::_1>>::inst_tuple;
	};

}	// namespace tl_oper

//----------------------------------------------------------------------
// MPL defs for Registering Classs RTTI
//----------------------------------------------------------------------

template<class TL> struct TypeListClassReg;

template<class... Ts>
struct TypeListClassReg<tl::type_list<Ts...>> {
	TypeListClassReg() {
		([]<class T>{ T::GetStaticClass(); } .template operator() < Ts > (), ...);
	}
};

#endif // __RTC_UTL_TYPELISTOPER_H
