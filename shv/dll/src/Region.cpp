
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

#include "ShvDllPch.h"

#include "Region.h"

#include "dbg/Debug.h"
#include "dllimp/RunDllProc.h"
#include "geo/geometry.h"
#include "ser/AsString.h"

#include "GeoTypes.h"
#include "geo/Conversions.h"

static const GRect s_WindowClipRect = GRect(-1024, -1024, 4096, 4096);
static const GRect s_EmptyRect      = GRect(0, 0, 0, 0);

#if defined(MG_DEBUG)

#define RGN_LOWERBOUND -16384
#define RGN_UPPERBOUND 16384

void CheckRgnLimits(const GRect& rect)
{
	dms_assert(rect.top  >= RGN_LOWERBOUND);
	dms_assert(rect.left >= RGN_LOWERBOUND);
	dms_assert(rect.bottom >= rect.top );
	dms_assert(rect.right  >= rect.left);
	dms_assert(RGN_UPPERBOUND >= rect.bottom);
	dms_assert(RGN_UPPERBOUND >= rect.right );
}

#endif

GRect ClipRect(const GRect& rect)
{
#if defined(MG_DEBUG)
	CheckRgnLimits(rect);
#endif
	GRect result = rect & s_WindowClipRect;
	if (result.empty())
		return s_EmptyRect;
	return result;
}

//----------------------------------------------------------------------
// class  : Region
//----------------------------------------------------------------------

Region::Region()
	:	m_Rgn(NULL, 0)
{}

Region::Region(HRGN hRgn)
	:	m_Rgn(hRgn)
{}

GRect ClipSize(GPoint size)
{
	dms_assert(size.x >= 0);
	dms_assert(size.y >= 0);

	MakeLowerBound(size, s_WindowClipRect.BottomRight());
	return GRect(GPoint(0,0), size);
}

HRGN CreateRectRgn(RECT&& rect)
{
	return CreateRectRgnIndirect(&rect);
}

Region::Region(GPoint size)
	:	m_Rgn( CreateRectRgn( ClipSize(size) ) )
{
}

Region::Region(const GRect& rect)
	:	m_Rgn( CreateRectRgn( ClipRect(rect) ) )
{
}		

Region::Region(HWND hWnd)
	:	m_Rgn( CreateRectRgn(0,0,0,0) )
{
	GetUpdateRgn(hWnd, m_Rgn, false);
}

Region::Region(HDC hdc, const TRect& rect)
{
	GRect clipRect;
	switch (GetClipBox(hdc, &clipRect))
	{
		case SIMPLEREGION:
		case COMPLEXREGION:
			clipRect &= rect;
			if (!clipRect.empty())
				*this = Region(clipRect);
			break;

		case ERROR:
		case NULLREGION:
			break;
	}
}

Region::Region(HDC hdc, HWND hWnd)
	:	m_Rgn( CreateRectRgn(0,0,0,0) )
{
	DBG_START("Region", "ctor(HDC, HWND)", MG_DEBUG_REGION);

	dms_assert(hWnd); // PRECONDITION

	GetRandomRgn(hdc, m_Rgn, SYSRGN);

	// The region returned by GetRandomRgn is in screen coordinates (excecpt in the unsuppoerted Windows 95/98/Me and the 16 bit Win 3.1
	(*this) += GPoint(0, 0).ScreenToClient(hWnd); // ::OffsetRgn(hRgn, pt.x, pt.y);

	DBG_TRACE(("Region = %s", AsString().c_str()));
}

Region::Region(Region&& src) noexcept // Move ctor
	:	m_Rgn(NULL, 0)
{
	m_Rgn.swap(src.m_Rgn);
}

Region::Region(const Region& src1, const Region& src2, int fCombineMode) // Combine
	:	m_Rgn( CreateRectRgn(0,0,0,0) )
{
	dms_assert( src1.m_Rgn!=0 );
	dms_assert((src2.m_Rgn!=0) == (fCombineMode != RGN_COPY));

	if (CombineRgn(m_Rgn, src1.m_Rgn, src2.m_Rgn, fCombineMode) == ERROR)
		throwLastSystemError("CombineRgn");
}

Region::~Region()
{
	Clear();
}

void Region::Clear() noexcept
{
	m_Rgn = GdiHandle<HRGN>();
}

void Region::swap(Region& oth) noexcept
{
	m_Rgn.swap(oth.m_Rgn);
}

Region Region::Clone() const
{
	if (!m_Rgn)
		return Region();
	return Region(*this, Region(), RGN_COPY);
}

GRect Region::BoundingBox() const
{
	dms_assert(m_Rgn!=0);

	GRect result;
	::GetRgnBox(m_Rgn, &result);
	return result;
}

bool Region::IsIntersecting(const GRect& rect) const
{
	if (!m_Rgn) 
		return false;

	return ::RectInRegion(m_Rgn, &rect);
}

bool Region::IsIntersecting(const Region& rhs) const
{
	if (!m_Rgn || !rhs.m_Rgn)
		return false;

	return ! Region(*this, rhs, RGN_AND).Empty();
}

bool Region::IsIncluding(const GRect& rect) const
{
	if (!m_Rgn)
		return false;

	return IsIncluding( Region(rect) );
}

bool Region::IsIncluding   (const Region& rhs) const
{
	if (!rhs.m_Rgn)
		return true;
	if (!m_Rgn)
		return false;

	return Region(*this, rhs, RGN_AND) == rhs;
}

bool Region::IsBoundedBy(const GRect& rect) const
{
	dms_assert(m_Rgn!=0);

	GRect bbox;
	
	return 
		GetRgnBox(m_Rgn, &bbox) == NULLREGION 
	||	::IsIncluding(
			Convert<IRect>(rect),
			Convert<IRect>(bbox)
		);
}

void Region::operator ^=(const Region&  src)
{
	if (m_Rgn)
	{
		if (src.m_Rgn)
			if (CombineRgn(m_Rgn, m_Rgn, src.m_Rgn, RGN_XOR) == ERROR)
				throwLastSystemError("CombineRgn");
	}
	else
		if (src.m_Rgn)
			operator = (Region(src, Region(), RGN_COPY) );
}

void Region::operator |=(const Region& src)
{
	if (m_Rgn)
	{
		if (src.m_Rgn)
			if (CombineRgn(m_Rgn, m_Rgn, src.m_Rgn, RGN_OR) == ERROR)
				throwLastSystemError("CombineRgn");
	}
	else
		if (src.m_Rgn)
			operator =(Region(src, Region(), RGN_COPY) );
}

void Region::operator &=(const Region&  src)
{
	if (!m_Rgn)
		return;
	if (!src.m_Rgn)
		m_Rgn = GdiHandle<HRGN>();
	else if (CombineRgn(m_Rgn, m_Rgn, src.m_Rgn, RGN_AND) == ERROR)
		throwLastSystemError("CombineRgn");
}

void Region::operator &=(const GRect&  src)
{
	operator &=( Region(src) );
}

void Region::operator -=(const Region&  src)
{
	if (!m_Rgn || !src.m_Rgn)
		return;

	if (CombineRgn(m_Rgn, m_Rgn, src.m_Rgn, RGN_DIFF) == ERROR)
		throwLastSystemError("CombineRgn");
}

void Region::operator +=(const GPoint& delta)
{
	if (m_Rgn)
		::OffsetRgn(m_Rgn, delta.x, delta.y);
}

bool Region::operator ==(const Region& rhs) const
{
	if (!m_Rgn)
		return rhs.Empty();
	if (!rhs.m_Rgn)
		return Empty();

	return ::EqualRgn(m_Rgn, rhs.m_Rgn);
}

bool Region::Empty() const
{
	if (!m_Rgn)
		return true;

	int result = CombineRgn(m_Rgn, m_Rgn, NULL, RGN_COPY);
	dms_assert(result != ERROR);
	return result == NULLREGION;
}

//	Scroll(delta, scrollRect, clipRgn)[pict]:
//	 resPict:= pict[scrollRect & clipRgn] + delta | pict[(~scrollRect & ~scrollRect+delta)| ~clipRgn]
//	 invReg := ((scrollRect & clipRgn) / (scrollRect+delta & clipRgn)) | (((scrollRect & !clipRgn) + delta) & clipRgn)
//	         = ((scrollRect / (scrollRect+delta)) & clipRgn) | (((scrollRect & !clipRgn) + delta) & clipRgn)
//	         = ((scrollRect / (scrollRect+delta)) | ((scrollRect & !clipRgn) + delta)) & clipRgn
void Region::Scroll(GPoint delta, const GRect& scrollRect, const Region& clipRgn)
{
	DBG_START("Region", "Scroll", MG_DEBUG_SCROLL);

	DBG_TRACE( ("OrgRegion : %s",  AsString().c_str() ) );

	dms_assert( IsIntersecting(clipRgn) ); // guaranteed by caller
//	dms_assert(scrollClipRgn.IsBoundedBy(scrollRect)); // guaranteed by caller that scrollRectRgn == Region(scrollRect) given for performance purposes

	if (IsBoundedBy(scrollRect))
	{
		DBG_TRACE(("Simple Scrolling: region is bounded by ScrollRect") );

		*this += delta;
		DBG_TRACE( ("After Offfsetting: %s",  AsString().c_str()) );

		*this &= clipRgn;
		DBG_TRACE( ("After Clipping   : %s",  AsString().c_str()) );
	}
	else
	{
		dms_assert(!clipRgn.Empty()); // guaranteed by IsIntersecting(clipRgn)
		if (!scrollRect.empty()) {
			DBG_TRACE(("Complex Scrolling: region is not bounded by ScrollRect") );

			Region scrolledRegion = Clone();
			DBG_TRACE( ("After Cloning    : %s",  AsString().c_str()) );
			DBG_TRACE( ("      Copy       : %s",  scrolledRegion.AsString().c_str()) );


			scrolledRegion &= scrollRect;
			scrolledRegion &= clipRgn;
			DBG_TRACE( ("After Clipping   : %s",  scrolledRegion.AsString().c_str()) );

			scrolledRegion += delta;
			DBG_TRACE( ("After Offfsetting: %s",  scrolledRegion.AsString().c_str()) );

			scrolledRegion &= clipRgn;
			DBG_TRACE( ("After Clipping   : %s",  scrolledRegion.AsString().c_str()) );

			*this -= Region(clipRgn, Region(scrollRect        ), RGN_AND);
			*this -= Region(clipRgn, Region(scrollRect + delta), RGN_AND);
			DBG_TRACE( ("SR removed       : %s",  AsString().c_str()) );

			*this |= scrolledRegion;
			DBG_TRACE( ("After Merging    : %s",  AsString().c_str()) );
		}
	}

	DBG_TRACE( ("ResRegion : %s",  AsString().c_str()) );
}

void Region::FillRectArray(RectArray& ra) const
{
	static std::vector<BYTE> rgnDataBuffer;
	UInt32 regionDataSize = GetRegionData(m_Rgn, 0, 0);
	rgnDataBuffer.resize( regionDataSize ); 

	RGNDATA* rgnData = reinterpret_cast<RGNDATA*>( &*rgnDataBuffer.begin() );

	GetRegionData(
		m_Rgn, 
		regionDataSize, 
		rgnData
	); 
	dms_assert(rgnData->rdh.iType  == RDH_RECTANGLES);
	dms_assert(rgnData->rdh.dwSize >= 32);

//	GRect* rects = reinterpret_cast<GRect*>( &(rgnData->Buffer[0]) );
	RECT*  rects = reinterpret_cast<RECT*>( &(rgnData->Buffer[0]) );

	UInt32 n = rgnData->rdh.nCount;

	ra.assign(rects, rects+n);
}

SharedStr Region::AsString() const
{
	RectArray ra;
	FillRectArray(ra);

	SharedStr result;
	for (UInt32 i = 0; i != ra.size(); ++i)
	{
		result += ::AsString(ra[i]) + "; ";
	}
	return result;
}
