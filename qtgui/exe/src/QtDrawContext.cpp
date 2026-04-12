// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "QtDrawContext.h"

#include "GeoTypes.h"
#include "Region.h"

#include <QPainter>
#include <QRect>
#include <QColor>
#include <QBrush>
#include <QPen>
#include <QFontMetrics>

static QColor DmsColor2QColor(DmsColor c)
{
	return QColor(GetRed(c), GetGreen(c), GetBlue(c), 255 - GetTrans(c));
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
	const QRegion& qrgn = rgn.GetQRegion();
	m_Painter->setClipRegion(qrgn);
	m_Painter->fillRect(qrgn.boundingRect(), DmsColor2QColor(color));
	m_Painter->setClipping(false);
}

void QtDrawContext::InvertRect(const GRect& rect)
{
	if (!m_Painter)
		return;
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

void QtDrawContext::InvertRegion(const Region& rgn)
{
	if (!m_Painter)
		return;
	const QRegion& qrgn = rgn.GetQRegion();
	auto oldMode = m_Painter->compositionMode();
	m_Painter->setCompositionMode(QPainter::CompositionMode_Difference);
	m_Painter->setClipRegion(qrgn);
	m_Painter->fillRect(qrgn.boundingRect(), Qt::white);
	m_Painter->setClipping(false);
	m_Painter->setCompositionMode(oldMode);
}

static Qt::PenStyle toQtPenStyle(DmsPenStyle style)
{
	switch (style) {
	case DmsPenStyle::Solid:      return Qt::SolidLine;
	case DmsPenStyle::Dash:       return Qt::DashLine;
	case DmsPenStyle::Dot:        return Qt::DotLine;
	case DmsPenStyle::DashDot:    return Qt::DashDotLine;
	case DmsPenStyle::DashDotDot: return Qt::DashDotDotLine;
	case DmsPenStyle::Null:       return Qt::NoPen;
	}
	return Qt::SolidLine;
}

void QtDrawContext::DrawLine(GPoint from, GPoint to, DmsColor color, int width)
{
	if (!m_Painter)
		return;
	QPen pen(DmsColor2QColor(color));
	pen.setWidth(width);
	auto oldPen = m_Painter->pen();
	m_Painter->setPen(pen);
	m_Painter->drawLine(from.x, from.y, to.x, to.y);
	m_Painter->setPen(oldPen);
}

void QtDrawContext::DrawPolyline(const GPoint* pts, int count, DmsColor color, int width, DmsPenStyle style)
{
	if (!m_Painter || count < 2)
		return;
	QVector<QPoint> qpts(count);
	for (int i = 0; i < count; ++i)
		qpts[i] = QPoint(pts[i].x, pts[i].y);
	QPen pen(DmsColor2QColor(color));
	pen.setWidth(width);
	pen.setStyle(toQtPenStyle(style));
	auto oldPen = m_Painter->pen();
	m_Painter->setPen(pen);
	m_Painter->drawPolyline(qpts.data(), count);
	m_Painter->setPen(oldPen);
}

static Qt::BrushStyle toQtHatchStyle(DmsHatchStyle hatch)
{
	switch (hatch) {
	case DmsHatchStyle::Solid:            return Qt::SolidPattern;
	case DmsHatchStyle::Horizontal:       return Qt::HorPattern;
	case DmsHatchStyle::Vertical:         return Qt::VerPattern;
	case DmsHatchStyle::ForwardDiagonal:  return Qt::FDiagPattern;
	case DmsHatchStyle::BackwardDiagonal: return Qt::BDiagPattern;
	case DmsHatchStyle::Cross:            return Qt::CrossPattern;
	case DmsHatchStyle::DiagonalCross:    return Qt::DiagCrossPattern;
	}
	return Qt::SolidPattern;
}

void QtDrawContext::DrawPolygon(const GPoint* pts, int count, DmsColor fillColor, DmsHatchStyle hatch)
{
	if (!m_Painter || count < 3)
		return;
	QVector<QPoint> qpts(count);
	for (int i = 0; i < count; ++i)
		qpts[i] = QPoint(pts[i].x, pts[i].y);

	auto oldPen = m_Painter->pen();
	auto oldBrush = m_Painter->brush();

	m_Painter->setPen(Qt::NoPen);
	QBrush brush(DmsColor2QColor(fillColor));
	brush.setStyle(toQtHatchStyle(hatch));
	m_Painter->setBrush(brush);
	m_Painter->drawPolygon(qpts.data(), count);

	m_Painter->setPen(oldPen);
	m_Painter->setBrush(oldBrush);
}

void QtDrawContext::DrawEllipse(const GRect& boundingRect, DmsColor color)
{
	if (!m_Painter)
		return;
	auto oldPen = m_Painter->pen();
	auto oldBrush = m_Painter->brush();
	m_Painter->setPen(Qt::NoPen);
	m_Painter->setBrush(DmsColor2QColor(color));
	m_Painter->drawEllipse(GRect2QRect(boundingRect));
	m_Painter->setPen(oldPen);
	m_Painter->setBrush(oldBrush);
}

void QtDrawContext::TextOut(GPoint pos, CharPtr text, int len, DmsColor color)
{
	if (!m_Painter)
		return;
	auto oldPen = m_Painter->pen();
	m_Painter->setPen(DmsColor2QColor(color));
	m_Painter->drawText(pos.x, pos.y + m_Painter->fontMetrics().ascent(), QString::fromUtf8(text, len));
	m_Painter->setPen(oldPen);
}

void QtDrawContext::TextOutW(GPoint pos, const wchar_t* text, int len, DmsColor color)
{
	if (!m_Painter)
		return;
	auto oldPen = m_Painter->pen();
	m_Painter->setPen(DmsColor2QColor(color));
	QString str = QString::fromWCharArray(text, len);
	int yOffset = m_CenterH ? 0 : m_Painter->fontMetrics().ascent();
	m_Painter->drawText(pos.x, pos.y + yOffset, str);
	m_Painter->setPen(oldPen);
}

void QtDrawContext::SetFont(CharPtr fontName, int pixelHeight, UInt16 angleDegTenths)
{
	if (!m_Painter)
		return;
	QFont font(QString::fromUtf8(fontName));
	font.setPixelSize(abs(pixelHeight));
	if (angleDegTenths)
	{
		QTransform t;
		t.rotate(-angleDegTenths / 10.0);
		// Qt doesn't directly support rotated fonts on QFont, rotation is handled via QPainter transform
	}
	m_Painter->setFont(font);
}

void QtDrawContext::SetTextAlign(bool centerH, bool baseline)
{
	m_CenterH = centerH;
	m_Baseline = baseline;
}

void QtDrawContext::DrawText(const GRect& rect, CharPtr text, int len, UInt32 format, DmsColor color)
{
	if (!m_Painter)
		return;
	auto oldPen = m_Painter->pen();
	m_Painter->setPen(DmsColor2QColor(color));

	int qtFlags = 0;
	if (format & 0x0001) qtFlags |= Qt::AlignHCenter; // DT_CENTER
	if (format & 0x0004) qtFlags |= Qt::AlignVCenter; // DT_VCENTER
	if (format & 0x0020) qtFlags |= Qt::TextSingleLine; // DT_SINGLELINE
	if (format & 0x0002) qtFlags |= Qt::AlignRight; // DT_RIGHT
	if (!(qtFlags & (Qt::AlignHCenter | Qt::AlignRight)))
		qtFlags |= Qt::AlignLeft;

	m_Painter->drawText(GRect2QRect(rect), qtFlags, QString::fromUtf8(text, len));
	m_Painter->setPen(oldPen);
}

GPoint QtDrawContext::GetTextExtent(CharPtr text, int len)
{
	if (!m_Painter)
		return GPoint(0, 0);
	QFontMetrics fm = m_Painter->fontMetrics();
	auto size = fm.size(0, QString::fromUtf8(text, len));
	return GPoint(size.width(), size.height());
}

void QtDrawContext::SetTextColor(DmsColor color)
{
	if (!m_Painter)
		return;
	m_Painter->setPen(DmsColor2QColor(color));
}

void QtDrawContext::SetBkColor(DmsColor color)
{
	if (!m_Painter)
		return;
	m_Painter->setBackground(QBrush(DmsColor2QColor(color)));
}

void QtDrawContext::SetBkMode(bool transparent)
{
	if (!m_Painter)
		return;
	m_Painter->setBackgroundMode(transparent ? Qt::TransparentMode : Qt::OpaqueMode);
}

GRect QtDrawContext::GetClipRect() const
{
	if (!m_Painter)
		return GRect(0, 0, 0, 0);
	QRect qr = m_Painter->clipBoundingRect().toRect();
	return GRect(qr.left(), qr.top(), qr.right(), qr.bottom());
}

void QtDrawContext::SetClipRegion(const Region& rgn)
{
	if (!m_Painter)
		return;
	m_Painter->setClipRegion(rgn.GetQRegion());
}

void QtDrawContext::SetClipRect(const GRect& rect)
{
	if (!m_Painter)
		return;
	m_Painter->setClipRect(GRect2QRect(rect));
}

void QtDrawContext::ResetClip()
{
	if (!m_Painter)
		return;
	m_Painter->setClipping(false);
}

void QtDrawContext::DrawImage(const GRect& destRect, const void* pixelData, int width, int height, int bitsPerPixel, const void* paletteRGBQuads, int paletteCount)
{
	if (!m_Painter || !pixelData || width <= 0 || height <= 0)
		return;

	// DIB is bottom-up; Qt QImage is top-down. Build a 32-bit ARGB image.
	QImage img(width, abs(height), QImage::Format_ARGB32);

	if (bitsPerPixel == 32)
	{
		// pixelData is BGRA (RGBQUAD order), bottom-up
		auto src = static_cast<const quint32*>(pixelData);
		for (int y = 0; y < abs(height); ++y)
		{
			int srcY = (height > 0) ? (abs(height) - 1 - y) : y; // bottom-up → top-down
			memcpy(img.scanLine(y), src + srcY * width, width * 4);
		}
	}
	else if (bitsPerPixel == 8 && paletteRGBQuads && paletteCount > 0)
	{
		struct RGBQ { quint8 b, g, r, reserved; };
		auto palette = static_cast<const RGBQ*>(paletteRGBQuads);
		auto src = static_cast<const quint8*>(pixelData);
		int stride = (width + 3) & ~3; // DWORD-aligned
		for (int y = 0; y < abs(height); ++y)
		{
			int srcY = (height > 0) ? (abs(height) - 1 - y) : y;
			auto* scanline = reinterpret_cast<quint32*>(img.scanLine(y));
			const quint8* row = src + srcY * stride;
			for (int x = 0; x < width; ++x)
			{
				quint8 idx = row[x];
				if (idx < paletteCount)
					scanline[x] = qRgba(palette[idx].r, palette[idx].g, palette[idx].b, 255);
				else
					scanline[x] = 0; // transparent for out-of-range
			}
		}
	}
	else
		return; // unsupported format

	m_Painter->drawImage(GRect2QRect(destRect), img);
}

static void DrawShadowRect(QPainter* p, GRect rect, QColor light, QColor dark)
{
	if (rect.top >= rect.bottom || rect.left >= rect.right)
		return;
	int nextLeft = rect.left + 1, nextTop = rect.top + 1;
	int prevRight = rect.right - 1, prevBottom = rect.bottom - 1;
	p->fillRect(QRect(prevRight, rect.top, 1, rect.bottom - rect.top), dark);
	p->fillRect(QRect(rect.left, prevBottom, prevRight - rect.left, 1), dark);
	if (rect.top >= prevBottom || rect.left >= prevRight) return;
	p->fillRect(QRect(rect.left, rect.top, prevRight - rect.left, 1), light);
	p->fillRect(QRect(rect.left, nextTop, 1, prevBottom - nextTop), light);
}

void QtDrawContext::DrawButtonBorder(GRect& rect)
{
	if (!m_Painter)
		return;
	DrawShadowRect(m_Painter, rect, QColor(227, 227, 227), QColor(64, 64, 64));
	rect.Shrink(1);
	DrawShadowRect(m_Painter, rect, QColor(255, 255, 255), QColor(160, 160, 160));
	rect.Shrink(1);
}

void QtDrawContext::DrawReversedBorder(GRect& rect)
{
	if (!m_Painter)
		return;
	DrawShadowRect(m_Painter, rect, QColor(64, 64, 64), QColor(227, 227, 227));
	rect.Shrink(1);
	DrawShadowRect(m_Painter, rect, QColor(160, 160, 160), QColor(255, 255, 255));
	rect.Shrink(1);
}
