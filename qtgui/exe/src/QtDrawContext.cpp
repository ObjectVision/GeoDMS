// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "QtDrawContext.h"

#include "GeoTypes.h"
#include "Region.h"

#include <QPainter>
#include <QRect>
#include <QColor>
#include <QBrush>

static QColor DmsColor2QColor(DmsColor c)
{
	return QColor(GetRed(c), GetGreen(c), GetBlue(c), 255 - GetTrans(c));
}

static QRect GRect2QRect(const GRect& r)
{
	return QRect(r.left, r.top, r.right - r.left, r.bottom - r.top);
}

void QtDrawContext::FillRect(const GRect& rect, DmsColor color)
{
	if (!m_Painter)
		return;
	m_Painter->fillRect(GRect2QRect(rect), DmsColor2QColor(color));
}

void QtDrawContext::FillRegion(const Region& rgn, DmsColor color)
{
	if (!m_Painter)
		return;
	// TODO: convert Region (HRGN) to QRegion for proper clipping.
	// For now, fill the bounding box of the region using Win32 GetRgnBox
	// to avoid a linker dependency on Region::BoundingBox() in shv.dll.
	GRect bbox;
	GetRgnBox(rgn.GetHandle(), &bbox);
	m_Painter->fillRect(GRect2QRect(bbox), DmsColor2QColor(color));
}

void QtDrawContext::InvertRect(const GRect& rect)
{
	if (!m_Painter)
		return;
	// QPainter doesn't have a direct InvertRect equivalent.
	// Use CompositionMode_Difference to approximate XOR inversion.
	auto oldMode = m_Painter->compositionMode();
	m_Painter->setCompositionMode(QPainter::CompositionMode_Difference);
	m_Painter->fillRect(GRect2QRect(rect), Qt::white);
	m_Painter->setCompositionMode(oldMode);
}

void QtDrawContext::DrawFocusRect(const GRect& rect)
{
	if (!m_Painter)
		return;
	QRect qr = GRect2QRect(rect);
	QPen pen(Qt::DashLine);
	pen.setColor(Qt::black);
	pen.setWidth(1);
	auto oldPen = m_Painter->pen();
	m_Painter->setPen(pen);
	m_Painter->drawRect(qr.adjusted(0, 0, -1, -1));
	m_Painter->setPen(oldPen);
}
