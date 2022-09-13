
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

#if !defined(__RTC_SER_STREAMTRAITS_H)
#define __RTC_SER_STREAMTRAITS_H

#include "RtcBase.h"

#include "utl/instantiate.h"

//----------------------------------------------------------------------
// Section      : struct is_binary_streamable, MPL style
//----------------------------------------------------------------------

template <typename T> struct is_binary_streamable;

#define INSTANTIATE(T) \
	template <> struct is_binary_streamable<T> : std::true_type {};
	INSTANTIATE_NUM_ELEM
	INSTANTIATE_BOOL
	INSTANTIATE(char)
#undef INSTANTIATE

template <> struct is_binary_streamable<CharPtr  > : std::false_type {};
template <> struct is_binary_streamable<TokenID  > : std::false_type {};
template <> struct is_binary_streamable<SharedStr> : std::false_type {};
template <> struct is_binary_streamable<Void     > : std::false_type {};

template <typename T> struct is_binary_streamable<std::vector<T> > : std::false_type {};

template<typename T, typename U> struct Pair;

template<typename T> struct is_binary_streamable<const T  >  : is_binary_streamable< T > {};
template<typename T> struct is_binary_streamable<Range<T>  > : is_binary_streamable< T > {};
template<typename T> struct is_binary_streamable<Point<T>  > : is_binary_streamable< T > {};
template<typename T> struct is_binary_streamable<Couple<T> > : is_binary_streamable< T > {};
template<typename T> struct is_binary_streamable<IndexRange<T> >: is_binary_streamable< T > {};
template<typename T, typename U> struct is_binary_streamable<Pair<T, U> > : std::bool_constant<is_binary_streamable<T>::value && is_binary_streamable<U>::value > {};

template<typename T> const bool is_binary_streamable_v = is_binary_streamable<T>::value;

//----------------------------------------------------------------------
// Section      : stream_tag maps streamability to one of the two tags
//----------------------------------------------------------------------

struct directcpy_streamable_tag;
struct polymorph_streamable_tag;


template <typename T>
using stream_tag_t = std::conditional_t<is_binary_streamable_v<T>, directcpy_streamable_tag, polymorph_streamable_tag>;

#define STREAMTYPE_TAG(T) TYPEID(stream_tag_t<const T>)

#if defined(MG_DEBUG)
	#include <boost/mpl/assert.hpp>

	BOOST_MPL_ASSERT((is_binary_streamable<UInt32>));
#endif // defined(MG_DEBUG)

#endif // !defined(__RTC_SER_STREAMTRAITS_H)
