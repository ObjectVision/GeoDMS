// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_CPC_TRANSFORM_H)
#define __RTC_CPC_TRANSFORM_H

//#define DMS_USE_TRANSFORMVIEW
//#define DMS_USE_JOINVIEW

#if !defined(DMS_USE_TRANSFORMVIEW) || !defined(DMS_USE_JOINVIEW)
#	include <boost/mpl/transform.hpp>
#	include <boost/mpl/vector/vector40.hpp>
#endif

#if defined(DMS_USE_TRANSFORMVIEW)
#	include <boost/mpl/transform_view.hpp>
#endif

#if defined(DMS_USE_JOINVIEW)
#	include <boost/mpl/joint_view.hpp>
#else
#	include <boost/mpl/placeholders.hpp>
#endif

//----------------------------------------------------------------------
// MPL style transformation
//----------------------------------------------------------------------

namespace tl {

	#if defined(DMS_USE_TRANSFORMVIEW)
		template <typename TL, typename F>
		struct transform: boost::mpl::transform_view <TL , F > {};

	#else

		template <typename TL, typename F>
		struct transform
			:	boost::mpl::transform1<TL , F, boost::mpl::back_inserter<boost::mpl::vector0<> > >::type 
		{};

	#endif

	#if defined(DMS_USE_JOINVIEW)
		template <typename TL1, typename TL2> struct jv2 : boost::mpl::joint_view<TL1, TL2> {};
	#else

		using namespace boost::mpl::placeholders;

		template <typename TL1, typename TL2>
		struct jv2
			:	boost::mpl::transform1<TL2 , _, boost::mpl::back_inserter<TL1> >::type 
		{};
	#endif

	template <typename TL1, typename TL2, typename TL3, typename TL4> struct jv4 : jv2<jv2<TL1, TL2>, jv2<TL3, TL4> > {};

}	//	namespace tl

#endif // __RTC_CPC_TRANSFORM_H
