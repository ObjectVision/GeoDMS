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

#if !defined(__RTC_MEM_RECTCOPY_H)
#define  __RTC_MEM_RECTCOPY_H

#include "mem/Grid.h"

// *****************************************************************************
//											row_operators
// *****************************************************************************

template <typename R, typename T=R>
struct fast_copier
{
//	using ResType = R;
//	using ArgType = T;

	using pointer = ptr_type_t<R>;
	using const_pointer = ptr_type_t<const T>;

	void operator() (const_pointer first, const_pointer last, pointer target) const
	{
		fast_copy(first, last, target);
	}
};

template <typename R, typename D>
struct fast_copier_and_const_adder
{
	D increment;

	using pointer = ptr_type_t<R>;
	using const_pointer = ptr_type_t<const R>;

	void operator() (const_pointer first, const_pointer last, pointer target) const
	{
		auto targetEnd = fast_copy(first, last, target);
		while (target != targetEnd)
			*target++ += increment;
	}
};

template <typename UnaryAssign>
struct row_assigner
{
	row_assigner(const UnaryAssign& assign = UnaryAssign()): m_Assign(assign) {}

	typedef typename UnaryAssign::arg1_type                  ArgType;
	typedef typename UnaryAssign::assignee_type              ResType;
	typedef typename sequence_traits<ArgType>::const_pointer ArgPtrType;
	typedef typename sequence_traits<ResType>::pointer       ResPtrType;
	void operator() (ArgPtrType first, ArgPtrType last, ResPtrType target) const
	{
		for (; first != last; ++target, ++first)
			m_Assign(*target, *first);
	}
	UnaryAssign m_Assign;
};

// *****************************************************************************
//											INTERFACE
// *****************************************************************************

template <typename R, typename T, typename Ux, typename RowOper>
void RectOper(TGridBase<R, Ux> dst, TGridBase<const T, Ux> src, Point<signed_type_t<Ux>> displacement, const RowOper& rowOper = RowOper())
{
	using TPoint = Point<signed_type_t<Ux>>;
	using TRect = Range<TPoint>;
	TRect overlapRect = (TRect(TPoint(0, 0), dst.GetSize()) + displacement) & TRect(TPoint(0, 0), src.GetSize()); // in src coordinates
	if(overlapRect.empty())
		return;

	Ux srcWidth = src.GetSize().Col(); dms_assert(srcWidth);
	Ux dstWidth = dst.GetSize().Col(); dms_assert(dstWidth);
	Ux nrCols = Width (overlapRect); dms_assert(nrCols);
	Ux nrRows = Height(overlapRect); dms_assert(nrRows);

	typename ptr_type<const T>::type srcPtr = src.begin() + SizeT(overlapRect.first.Row()) * srcWidth + overlapRect.first.Col();
	overlapRect -= displacement;
	typename ptr_type<R>::type dstPtr = dst.begin() + SizeT(overlapRect.first.Row()) * dstWidth + overlapRect.first.Col();

	for (Ux r=0; r!=nrRows; ++r, srcPtr += srcWidth, dstPtr += dstWidth)
		rowOper(srcPtr, srcPtr+nrCols, dstPtr);
}

template <typename R, typename T, typename Ux>
void RectCopy(TGridBase<R, Ux> dst, TGridBase<const T, Ux> src, Point<signed_type_t<Ux>> displacement)
{
	RectOper<R, T, Ux>(dst, src, displacement, fast_copier<R, T>{} );
}

template <typename R, typename Ux, typename D>
void RectCopyAndAddConst(TGridBase<R, Ux> dst, TGridBase<const R, Ux> src, Point<signed_type_t<Ux>> displacement, D increment)
{
	RectOper<R, R, Ux>(dst, src, displacement, fast_copier_and_const_adder<R, D>{increment});
}

#endif //!defined(__RTC_MEM_RECTCOPY_H)
