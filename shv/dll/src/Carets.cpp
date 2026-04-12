// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "dbg/debug.h"
#include "dbg/DebugCast.h"
#include "geo/PointOrder.h"
#include "geo/Undefined.h"
#include "mci/Class.h"
#include "utl/Environment.h"

#include "ShvUtils.h"
#include "Carets.h"
#include "DrawContext.h"

#include "CaretOperators.h"

#include <vector>

void AbstrCaret::Move(const AbstrCaretOperator& caret_operator, DrawContext& dc)
{
	DBG_START("AbstrCaret", "Move", MG_DEBUG_CARET);

	if (IsVisible()) Reverse(dc, false);
	caret_operator(this);
	if (IsVisible()) Reverse(dc, true);
}

bool AbstrCaret::IsVisible() const
{
	return IsDefined(m_StartPoint); 
}

//----------------------------------------------------------------------
// class  : NeedleCaret
//----------------------------------------------------------------------

NeedleCaret::NeedleCaret()
:	m_ViewRect(UNDEFINED_VALUE(GPoint), UNDEFINED_VALUE(GPoint))
{}

void NeedleCaret::Reverse(DrawContext& dc, bool newVisibleState)
{
	DBG_START("NeedleCaret", "Reverse", MG_DEBUG_CARET);

	GRect clipRect = dc.GetClipRect();
	if (clipRect.empty())
		return;

	GRect viewRect = m_ViewRect & clipRect;
	if (viewRect.empty())
		return;

	if (m_StartPoint.y != -1)
		dc.DrawLine(GPoint(viewRect.left, m_StartPoint.y), GPoint(viewRect.right, m_StartPoint.y), CombineRGB(0, 0, 0));
	if (m_StartPoint.x != -1)
		dc.DrawLine(GPoint(m_StartPoint.x, viewRect.top), GPoint(m_StartPoint.x, viewRect.bottom), CombineRGB(0, 0, 0));
}

void NeedleCaret::Move(const AbstrCaretOperator& caret_operator, DrawContext& dc)
{
	GPoint prevStartPoint = m_StartPoint;
	GRect  prevRect       = m_ViewRect;

	bool wasVisible = IsVisible();

	caret_operator(this);

	bool nowVisible = IsVisible();

	bool rectChanged = (prevRect != m_ViewRect);

	if (m_StartPoint != prevStartPoint || rectChanged )
	{
		GRect clipRect = dc.GetClipRect();
		assert(IsDefined(clipRect.left) && IsDefined(clipRect.top) && IsDefined(clipRect.right) && IsDefined(clipRect.bottom));
		if (wasVisible)
		{
			assert(IsDefined(prevRect));
			prevRect &= clipRect;
		}
		GRect viewRect = m_ViewRect;
		if (nowVisible)
		{
			assert(IsDefined(viewRect));
			viewRect &= clipRect;
		}

		DmsColor lineColor = CombineRGB(0, 0, 0);

		GType currRow = m_StartPoint.y;
		GType prevRow   = prevStartPoint.y;
		if (currRow != prevRow || rectChanged)
		{
			if (wasVisible)
				dc.DrawLine(GPoint(prevRect.left, prevRow), GPoint(prevRect.right, prevRow), lineColor);
			if (nowVisible)
				dc.DrawLine(GPoint(viewRect.left, currRow), GPoint(viewRect.right, currRow), lineColor);
		}
		GType currCol = m_StartPoint.x;
		GType prevCol   = prevStartPoint.x;
		if (currCol != prevCol || rectChanged)
		{
			if (wasVisible)
				dc.DrawLine(GPoint(prevCol, prevRect.top), GPoint(prevCol, prevRect.bottom), lineColor);
			if (nowVisible)
				dc.DrawLine(GPoint(currCol, viewRect.top), GPoint(currCol, viewRect.bottom), lineColor);
		}
	}
}

//----------------------------------------------------------------------
// class  : DualPointCaret
//----------------------------------------------------------------------

DualPointCaret::DualPointCaret()
:	m_EndPoint  ( UNDEFINED_VALUE(GPoint) ) 
{}

//----------------------------------------------------------------------
// class  : FocusCaret
//----------------------------------------------------------------------

void FocusCaret::Reverse(DrawContext& dc, bool newVisibleState)
{
	DBG_START("FocusCaret", "Reverse", MG_DEBUG_CARET);

#if defined(MG_DEBUG)
	if	(	m_StartPoint.x != m_EndPoint.x
		&&	m_StartPoint.y != m_EndPoint.y)
	{
		GRect rect(m_StartPoint, m_EndPoint);
		dc.DrawFocusRect(rect);
	}
#endif
}


GRect FocusCaret::GetRect() const
{
	return GRect(m_StartPoint, m_EndPoint ); 
}

//----------------------------------------------------------------------
// class  : LineCaret
//----------------------------------------------------------------------

void LineCaret::Reverse(DrawContext& dc, bool newVisibleState)
{
	dc.DrawLine(m_StartPoint, m_EndPoint, m_LineColor, 2);
}

//----------------------------------------------------------------------
// class  : PolygonCaret
//----------------------------------------------------------------------

void PolygonCaret::Reverse(DrawContext& dc, bool newVisibleState)
{
	dc.DrawPolygon(m_First, m_Count, COLORREF2DmsColor(m_FillColor));
}

//----------------------------------------------------------------------
// class  :	InvertRgnCaret
//----------------------------------------------------------------------
#include "Region.h"

// override AbstrCaret interface
void InvertRgnCaret::Reverse(DrawContext& dc, bool newVisibleState)
{
	DBG_START("InvertRgnCaret", "Reverse", MG_DEBUG_CARET);

	Region rgn;
	GetRgn(rgn, dc);
	dc.InvertRegion(rgn);
}

void InvertRgnCaret::Move(const AbstrCaretOperator& caret_operator, DrawContext& dc)
{
	Region orgRegion;

	if (IsVisible())
		GetRgn(orgRegion, dc);

	caret_operator(this);

	Region newRegion;
	if (IsVisible())
	{
		GetRgn(newRegion, dc);
		orgRegion ^= newRegion;
	}
	if (!orgRegion.Empty())
		dc.InvertRegion(orgRegion);
}

//----------------------------------------------------------------------
// class  : RectCaret
//----------------------------------------------------------------------

// override InvertRgnCaret interface
void RectCaret::GetRgn(Region& rgn, DrawContext& dc) const
{
	rgn = 
		Region( 
			GRect(
				LowerBound(m_StartPoint, m_EndPoint),
				UpperBound(m_StartPoint, m_EndPoint)
			) 
		);
}

//----------------------------------------------------------------------
// class  : MovableRectCaret
//----------------------------------------------------------------------

MovableRectCaret::MovableRectCaret(GRect objRect)
	:	m_ObjRect(objRect)
{
	if (m_ObjRect.Width() > 4 && m_ObjRect.Height() > 4)
		m_SubRect = GRect(m_ObjRect.Left()+2, m_ObjRect.Top()+2, m_ObjRect.Right()-2, m_ObjRect.Bottom()-2);
}

void MovableRectCaret::GetRgn(Region& rgn, DrawContext& dc) const
{
	if (m_StartPoint != m_EndPoint)
	{
		GPoint diff = GPoint(m_EndPoint) - GPoint(m_StartPoint);
		GRect clipRect = dc.GetClipRect();
		GRect clipped = clipRect & (m_ObjRect + diff);
		if (!clipped.empty())
			rgn = Region(clipped);
		if (!m_SubRect.empty())
		{
			GRect subClipped = clipRect & (m_SubRect + diff);
			if (!subClipped.empty())
				rgn ^= Region(subClipped);
		}
	}
}

//----------------------------------------------------------------------
// class  : BoundaryCaret
//----------------------------------------------------------------------

#include "MovableObject.h"

BoundaryCaret::BoundaryCaret(MovableObject* obj)
	:	MovableRectCaret(CrdRect2GRect(obj->GetCurrClientAbsDeviceRect()))
{}

//----------------------------------------------------------------------
// class  : RoiCaret
//----------------------------------------------------------------------

#include "geo/Conversions.h"

#include "ViewPort.h"

// override InvertRgnCaret interface
void RoiCaret::GetRgn(Region& rgn, DrawContext& dc) const
{
	RectCaret::GetRgn(rgn, dc);

	if (m_StartPoint != m_EndPoint)
	{
		const ViewPort* vp = dynamic_cast<const ViewPort*>(m_UsedObject);
		MG_CHECK(vp);

		auto rubberBandDRect = DRect(shp2dms_order(m_StartPoint.x, m_StartPoint.y), shp2dms_order(m_EndPoint.x, m_EndPoint.y));

		auto sf = CrdPoint(1.0, 1.0); // TODO: get DPI scale from ViewHost when available

		auto vpLogicalSize  = CrdRect(CrdPoint(0, 0), Convert<CrdPoint>(vp->GetCurrClientSize()));
		auto vpDeviceSize = DRect2GRect(vpLogicalSize, CrdTransformation(CrdPoint(0.0, 0.0), sf));
		auto vpDeviceSizeAsDRect = DRect(shp2dms_order(vpDeviceSize.left, vpDeviceSize.top), shp2dms_order(vpDeviceSize.right, vpDeviceSize.bottom));

		auto rb2vTr = CrdTransformation(rubberBandDRect, vpLogicalSize, OrientationType::Default);
		auto dvp = rb2vTr.Reverse(vpLogicalSize);
		auto viewPortTRect = Inflate<TPoint>(Convert<TRect>(dvp), Point<TType>(1, 1));
		auto viewPortGRect = TRect2GRect(viewPortTRect, CrdPoint(1.0, 1.0));

		rgn ^= Region(viewPortGRect);
	}
}

//----------------------------------------------------------------------
// class  : CircleCaret
//----------------------------------------------------------------------

GType CircleCaret::Radius() const
{
	auto sqrDist = SqrDist<GType>(m_EndPoint, m_StartPoint);
	return sqrt(sqrDist);
}

// override InvertRgnCaret interface
void CircleCaret::GetRgn(Region& rgn, DrawContext& dc) const
{
	auto radius = Radius();

	rgn = Region::FromEllipse(
		GRect(
			m_StartPoint.x - radius, m_StartPoint.y - radius,
			m_StartPoint.x + radius, m_StartPoint.y + radius
		)
	);
}


