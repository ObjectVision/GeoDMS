// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


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
		for (; target != targetEnd; ++target)
			if (IsDefined(*target))
				*target += increment;
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
