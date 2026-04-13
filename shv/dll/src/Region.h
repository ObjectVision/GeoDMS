// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__SHV_REGION_H)
#define __SHV_REGION_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "ShvUtils.h"

#include <QRegion>
#include <QRect>

using RectArray = std::vector<GRect>;

#if defined(MG_DEBUG)
void CheckRgnLimits(const GRect& rect);
#endif

//----------------------------------------------------------------------
// GRect <-> QRect conversion helpers
//----------------------------------------------------------------------

inline QRect GRect2QRect(const GRect& r)
{
	return QRect(r.left, r.top, r.right - r.left, r.bottom - r.top);
}

inline GRect QRect2GRect(const QRect& r)
{
	return GRect(r.x(), r.y(), r.x() + r.width(), r.y() + r.height());
}

//----------------------------------------------------------------------
// class  : Region
//----------------------------------------------------------------------

struct SHV_CALL Region
{
	Region();
	explicit Region(GPoint size);
	explicit Region(const GRect& rect);
	explicit Region(QRegion qrgn);

	Region(const Region& src) = default;
	Region(Region&& src) noexcept = default;
	~Region() = default;

	Region& operator =(const Region& rhs) = default;
	Region& operator =(Region&& rhs) noexcept = default;

	void Clear() noexcept;

	void swap(Region& oth) noexcept;
	Region Clone() const; 

	GRect BoundingBox() const;

	bool IsIntersecting(const GRect& rect) const;
	bool IsIntersecting(const Region& rhs) const;
	bool IsIncluding   (const GRect& rect) const;
	bool IsIncluding   (const Region& rhs) const;
	bool IsBoundedBy   (const GRect& rect) const;

	void operator ^=(const Region& src);
	void operator |=(const Region& src);
	void operator &=(const Region& src);
	void operator &=(const GRect&  src);

	void operator -=(const Region& src);

	void operator +=(const GPoint& delta);
	bool operator ==(const Region& rhs) const;
	bool operator !=(const Region& rhs) const { return !operator ==(rhs); }
	bool Empty() const;

	void ScrollDevice(GPoint delta, const GRect& scrollRect, const Region& scrollClipRgn);

	void FillRectArray(RectArray& ra) const;
	SharedStr AsString() const;

	const QRegion& GetQRegion() const { return m_Rgn; }

	static Region FromEllipse(const GRect& boundingRect);

private:
	QRegion m_Rgn;
};

inline Region operator &(const Region& a, const Region& b)
{
	if (a.Empty() || b.Empty())
		return Region();

	return Region(a.GetQRegion().intersected(b.GetQRegion()));
}

inline Region operator |(const Region& a, const Region& b)
{
	if (a.Empty()) return b;
	if (b.Empty()) return a;

	return Region(a.GetQRegion().united(b.GetQRegion()));
}

inline Region operator ^(const Region& a, const Region& b)
{
	if (a.Empty()) return b;
	if (b.Empty()) return a;

	return Region(a.GetQRegion().xored(b.GetQRegion()));
}

inline Region operator -(const Region& a, const Region& b)
{
	if (a.Empty())
		return Region();
	if (b.Empty())
		return a;

	return Region(a.GetQRegion().subtracted(b.GetQRegion()));
}

//----------------------------------------------------------------------
// ClipRegionSelector
// Modifies DrawContext clip region on construction, restores on destruction.
//----------------------------------------------------------------------

class DrawContext;

struct DcClipRegionSelector
{
	DcClipRegionSelector(DrawContext* dc, Region& currClipRegion, const GRect& newClipRect);
	~DcClipRegionSelector();

	bool empty() const { return m_OrgRegionPtr->Empty(); }

private:
	DrawContext* m_DC;
	Region*      m_OrgRegionPtr;
	Region       m_OrgRegionCopy;
};

#endif // __SHV_REGION_H

