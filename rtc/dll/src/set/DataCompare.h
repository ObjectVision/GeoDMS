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
#if !defined(__RTC_SET_DATACOMPARE_H)
#define __RTC_SET_DATACOMPARE_H

#include "geo/Undefined.h"

// *****************************************************************************
//                      INDEX operations
// *****************************************************************************

template <typename T, bool CheckUndefined> struct DataCompareImpl;

template <typename T> struct DataCompareImpl<T, false> {
	using is_transparent = int;

	template <typename U1, typename U2>
	bool operator()(U1&& left, U2&& right) const
	{
		return std::forward<U1>(left) < std::forward<U2>(right);
	}
};

template <typename T> struct DataCompareImpl<T, true> {
	using is_transparent = int;

	template <typename U1, typename U2>
	bool operator()(U1&& left, U2&& right) const
	{
		return IsDefined(right) && ((left < right) || !IsDefined(left));
	}
};

template <typename T>
struct DataCompare : DataCompareImpl<T, has_undefines_v<T> && !has_min_as_null_v<T> && has_fixed_elem_size_v<T>> {};

template <typename T>
struct DataCompare<Point<T> >
{
	using is_transparent = int;

	template <typename U1, typename U2>
	bool operator()(U1&& left, U2&& right) const
	{
		return m_ElemComp(left.Row(), right.Row())
			||	!m_ElemComp(right.Row(), left.Row())
			&& m_ElemComp(left.Col(), right.Col())
			;
	}
	DataCompare<T> m_ElemComp;
};


#endif // __RTC_SET_DATACOMPARE_H
