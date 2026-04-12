// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "Region.h"

#include "dbg/debug.h"
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
	assert(rect.top  >= RGN_LOWERBOUND);
	assert(rect.left >= RGN_LOWERBOUND);
	assert(rect.bottom >= rect.top );
	assert(rect.right  >= rect.left);
	assert(RGN_UPPERBOUND >= rect.bottom);
	assert(RGN_UPPERBOUND >= rect.right );
}

#endif

GRect ClipRect(const GRect& rect)
{
	GRect result = rect & s_WindowClipRect;
	if (result.empty())
		return s_EmptyRect;
	return result;
}

//----------------------------------------------------------------------
// class  : Region
//----------------------------------------------------------------------

Region::Region()
{}

static QRegion ClipSizeToQRegion(GPoint size)
{
	dms_assert(size.x >= 0);
	dms_assert(size.y >= 0);
	MakeLowerBound(size, s_WindowClipRect.RightBottom());
	if (size.x <= 0 || size.y <= 0)
		return QRegion();
	return QRegion(QRect(0, 0, size.x, size.y));
}

Region::Region(GPoint size)
	: m_Rgn(ClipSizeToQRegion(size))
{
}

Region::Region(const GRect& rect)
{
	GRect clipped = ClipRect(rect);
	if (!clipped.empty())
		m_Rgn = QRegion(GRect2QRect(clipped));
}

Region::Region(QRegion qrgn)
	: m_Rgn(std::move(qrgn))
{
}

void Region::Clear() noexcept
{
	m_Rgn = QRegion();
}

void Region::swap(Region& oth) noexcept
{
	m_Rgn.swap(oth.m_Rgn);
}

Region Region::Clone() const
{
	return *this; // QRegion is implicitly shared (copy-on-write)
}

GRect Region::BoundingBox() const
{
	assert(!m_Rgn.isEmpty());
	return QRect2GRect(m_Rgn.boundingRect());
}

bool Region::IsIntersecting(const GRect& rect) const
{
	if (m_Rgn.isEmpty()) 
		return false;
	return m_Rgn.intersects(GRect2QRect(rect));
}

bool Region::IsIntersecting(const Region& rhs) const
{
	if (m_Rgn.isEmpty() || rhs.m_Rgn.isEmpty())
		return false;
	return m_Rgn.intersects(rhs.m_Rgn);
}

bool Region::IsIncluding(const GRect& rect) const
{
	if (m_Rgn.isEmpty())
		return false;
	return IsIncluding(Region(rect));
}

bool Region::IsIncluding(const Region& rhs) const
{
	if (rhs.m_Rgn.isEmpty())
		return true;
	if (m_Rgn.isEmpty())
		return false;
	// a includes b iff (a & b) == b
	return m_Rgn.intersected(rhs.m_Rgn) == rhs.m_Rgn;
}

bool Region::IsBoundedBy(const GRect& rect) const
{
	if (m_Rgn.isEmpty())
		return true;

	QRect bbox = m_Rgn.boundingRect();
	QRect qrect = GRect2QRect(rect);
	return qrect.contains(bbox);
}

void Region::operator ^=(const Region& src)
{
	if (m_Rgn.isEmpty())
		m_Rgn = src.m_Rgn;
	else if (!src.m_Rgn.isEmpty())
		m_Rgn = m_Rgn.xored(src.m_Rgn);
}

void Region::operator |=(const Region& src)
{
	if (m_Rgn.isEmpty())
		m_Rgn = src.m_Rgn;
	else if (!src.m_Rgn.isEmpty())
		m_Rgn = m_Rgn.united(src.m_Rgn);
}

void Region::operator &=(const Region& src)
{
	if (m_Rgn.isEmpty())
		return;
	if (src.m_Rgn.isEmpty())
		m_Rgn = QRegion();
	else
		m_Rgn = m_Rgn.intersected(src.m_Rgn);
}

void Region::operator &=(const GRect& src)
{
	operator &=(Region(src));
}

void Region::operator -=(const Region& src)
{
	if (m_Rgn.isEmpty() || src.m_Rgn.isEmpty())
		return;
	m_Rgn = m_Rgn.subtracted(src.m_Rgn);
}

void Region::operator +=(const GPoint& delta)
{
	if (!m_Rgn.isEmpty())
		m_Rgn.translate(delta.x, delta.y);
}

bool Region::operator ==(const Region& rhs) const
{
	if (m_Rgn.isEmpty())
		return rhs.Empty();
	if (rhs.m_Rgn.isEmpty())
		return Empty();
	return m_Rgn == rhs.m_Rgn;
}

bool Region::Empty() const
{
	return m_Rgn.isEmpty();
}

//	ScrollDevice(delta, scrollRect, clipRgn)[pict]:
//	 resPict:= pict[scrollRect & clipRgn] + delta | pict[(~scrollRect & ~scrollRect+delta)| ~clipRgn]
//	 invReg := ((scrollRect & clipRgn) / (scrollRect+delta & clipRgn)) | (((scrollRect & !clipRgn) + delta) & clipRgn)
//	         = ((scrollRect / (scrollRect+delta)) & clipRgn) | (((scrollRect & !clipRgn) + delta) & clipRgn)
//	         = ((scrollRect / (scrollRect+delta)) | ((scrollRect & !clipRgn) + delta)) & clipRgn
void Region::ScrollDevice(GPoint delta, const GRect& scrollRect, const Region& clipRgn)
{
	DBG_START("Region", "Scroll", MG_DEBUG_SCROLL);

	DBG_TRACE( ("OrgRegion : %s",  AsString().c_str() ) );

	dms_assert( IsIntersecting(clipRgn) ); // guaranteed by caller

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

			*this -= Region(clipRgn.GetQRegion().intersected(Region(scrollRect        ).GetQRegion()));
			*this -= Region(clipRgn.GetQRegion().intersected(Region(scrollRect + delta).GetQRegion()));
			DBG_TRACE( ("SR removed       : %s",  AsString().c_str()) );

			*this |= scrolledRegion;
			DBG_TRACE( ("After Merging    : %s",  AsString().c_str()) );
		}
	}

	DBG_TRACE( ("ResRegion : %s",  AsString().c_str()) );
}

void Region::FillRectArray(RectArray& ra) const
{
	ra.clear();
	if (m_Rgn.isEmpty())
		return;

	for (const QRect& qr : m_Rgn)
		ra.push_back(QRect2GRect(qr));
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

Region Region::FromEllipse(const GRect& boundingRect)
{
	QRect qr = GRect2QRect(boundingRect);
	return Region(QRegion(qr, QRegion::Ellipse));
}
