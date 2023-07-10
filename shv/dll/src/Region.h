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

#if !defined(__SHV_REGION_H)
#define __SHV_REGION_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "DcHandle.h"
#include "ShvUtils.h"

typedef std::vector<GRect> RectArray;

#if defined(MG_DEBUG)
void CheckRgnLimits(const GRect& rect);
#endif

//----------------------------------------------------------------------
// class  : Region
//----------------------------------------------------------------------

struct Region
{
	Region();
	explicit Region(HRGN hRgn);
	explicit Region(HWND hWnd);
	explicit Region(GPoint size);
	explicit Region(const GRect&  rect);
	explicit Region(HDC hdc, const TRect& rect);
	explicit Region(HDC hdc, HWND hWnd);

	Region(Region&& src) noexcept; // Move ctor

	Region(const Region& src1, const Region& src2, int fCombineMode);

	~Region();
	
	void Clear() noexcept;

	void swap(Region& oth) noexcept;
	Region Clone() const; 

	GRect BoundingBox(HDC dc) const;

	bool IsIntersecting(const GRect& rect) const;
	bool IsIntersecting(const Region& rhs) const;
	bool IsIncluding   (const GRect& rect) const;
	bool IsIncluding   (const Region& rhs) const;
	bool IsBoundedBy   (const GRect& rect) const;

	void operator = (Region&& rhs) noexcept { Clear(); swap(rhs); }

	void operator ^=(const Region& src); // RGN_XOR
	void operator |=(const Region& src); // RGN_OR
	void operator &=(const Region& src); // RGN_AND
	void operator &=(const GRect&  src);

	void operator -=(const Region&  src);

	void operator +=(const GPoint& delta);
	bool operator ==(const Region& rhs) const;
	bool operator !=(const Region& rhs) const { return !operator ==(rhs); }
	bool Empty() const;

	void Scroll(GPoint delta, const GRect& scrollRect, const Region& scrollClipRgn);

	void FillRectArray(RectArray& ra) const;
	SharedStr AsString() const;


	HRGN GetHandle() const { return m_Rgn; }

private:
	Region(const Region&); // no implementation
	GdiHandle<HRGN> m_Rgn;
};

inline Region operator &(const Region& a, const Region& b)
{
	if (a.Empty() || b.Empty())
		return Region();

	return Region(a, b, RGN_AND);
}

inline Region operator |(const Region& a, const Region& b)
{
	if (a.Empty()) return b.Clone();
	if (b.Empty()) return a.Clone();

	return Region(a, b, RGN_OR);
}

inline Region operator ^(const Region& a, const Region& b)
{
	if (a.Empty()) return b.Clone();
	if (b.Empty()) return a.Clone();

	return Region(a, b, RGN_XOR);
}

inline Region operator -(const Region& a, const Region& b)
{
	if (a.Empty() || b.Empty())
		return Region();
	if (b.Empty() )
		return a.Clone();

	return Region(a, b, RGN_DIFF);
}

//----------------------------------------------------------------------
// DcClipRegionSelector
//----------------------------------------------------------------------

struct DcClipRegionSelector
{
	DcClipRegionSelector(HDC hdc, Region& currClipRegion, const GRect& newClipRect);
	~DcClipRegionSelector();

	bool empty() const { return m_OrgRegionPtr->Empty(); }

private:
	HDC     m_hDC;
	Region* m_OrgRegionPtr;
	Region  m_OrgRegionCopy;
};

#endif // __SHV_REGION_H

