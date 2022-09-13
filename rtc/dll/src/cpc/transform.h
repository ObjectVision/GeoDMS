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
