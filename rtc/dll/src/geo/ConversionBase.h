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

#ifndef __RTC_GEO_CONVERSIONBASE_H
#define __RTC_GEO_CONVERSIONBASE_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "RtcBase.h"

//----------------------------------------------------------------------
// Convert function wrapper
//----------------------------------------------------------------------

struct undefined_or_zero_func
{
	template <typename U, typename T>
	U apply(const T& v) const
	{
		return UNDEFINED_OR_ZERO(U);
	}
};

struct throw_func
{
	template <typename U, typename T>
	[[noreturn]] U apply(const T& v) const
	{
		throwErrorF("Convert", "Failed to convert %s to %s", v, typeid(U).name());
	}
};

template<typename Dst> struct DefaultConvertFunc;

template<typename Dst, typename Src> inline
Dst Convert(const Src& src)
{
	typedef DefaultConvertFunc<Dst> ConvertFunc;
	return Convert4(src, TYPEID(Dst), TYPEID(undefined_or_zero_func), TYPEID(ConvertFunc));
}

template<typename Dst, typename Src> inline
Dst ThrowingConvert(const Src& src)
{
	typedef DefaultConvertFunc<Dst> ConvertFunc;
	return Convert4(src, TYPEID(Dst), TYPEID(throw_func), TYPEID(ConvertFunc));
}

template<typename Dst, typename Src> inline
Dst ThrowingConvertNonNull(const Src& src)
{
	typedef DefaultConvertFunc<Dst> ConvertFunc;
	return ConvertNonNull4(src, TYPEID(Dst), TYPEID(throw_func), TYPEID(ConvertFunc));
}


#endif // __RTC_GEO_CONVERSIONBASE_H
