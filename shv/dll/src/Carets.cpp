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

#include "dbg/Debug.h"
#include "dbg/DebugCast.h"
#include "geo/PointOrder.h"
#include "geo/Undefined.h"
#include "mci/Class.h"
#include "utl/Environment.h"

#include "ShvUtils.h"
#include "Carets.h"

#include "CaretOperators.h"

#include <vector>

inline void MoveTo(HDC dc, GPoint p)
{
	MoveToEx(dc, p.x, p.y, NULL);
}

inline void LineTo(HDC dc, GPoint p)
{
	LineTo  (dc, p.x, p.y);
}

AbstrCaret::AbstrCaret()
	:	m_StartPoint( UNDEFINED_VALUE(GPoint) )
	,	m_UsedObject(0)
{}

void AbstrCaret::Move(const AbstrCaretOperator& caret_operator, HDC dc)
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

void NeedleCaret::Reverse(HDC dc, bool newVisibleState)
{
	DBG_START("NeedleCaret", "Reverse", MG_DEBUG_CARET);

	dms_assert(dc);

	RECT clipRect;
	int clipStatus = GetClipBox(dc, &clipRect);
	if (clipStatus == NULLREGION)
		return;
	if (clipStatus == ERROR)
		throwLastSystemError("NeedleCaret::Reverse");

	GRect viewRect = m_ViewRect & clipRect;
	if (viewRect.empty())
		return;

	if (m_StartPoint.y != -1)
	{
		MoveToEx(dc, viewRect.left,  m_StartPoint.y, NULL);
		LineTo  (dc, viewRect.right, m_StartPoint.y);
	}
	if (m_StartPoint.x != -1)
	{
		MoveToEx(dc, m_StartPoint.x, viewRect.top,   NULL);
		LineTo  (dc, m_StartPoint.x, viewRect.bottom);
	}
}

void NeedleCaret::Move(const AbstrCaretOperator& caret_operator, HDC dc)
{
	GPoint prevStartPoint = m_StartPoint;
	GRect  prevRect       = m_ViewRect;

	bool wasVisible = IsVisible();

	caret_operator(this);

	bool nowVisible = IsVisible();

	bool rectChanged = (prevRect != m_ViewRect);

	if (m_StartPoint != prevStartPoint || rectChanged )
	{
		RECT clipRect; GetClipBox(dc, &clipRect);
		      prevRect &= clipRect;
		GRect viewRect  = m_ViewRect & clipRect;

		GType currRow = m_StartPoint.y;
		GType prevRow   = prevStartPoint.y;
		if (currRow != prevRow || rectChanged)
		{
			if (wasVisible)
			{
				MoveToEx(dc, prevRect.left, prevRow, NULL);
				LineTo  (dc, prevRect.right, prevRow);
			}
			if (nowVisible)
			{
				MoveToEx(dc, viewRect.left, currRow, NULL);
				LineTo  (dc, viewRect.right, currRow);
			}
		}
		GType currCol = m_StartPoint.x;
		GType prevCol   = prevStartPoint.x;
		if (currCol != prevCol || rectChanged)
		{
			if (wasVisible)
			{
				MoveToEx(dc, prevCol,   prevRect.top,    NULL);
				LineTo  (dc, prevCol,   prevRect.bottom);
			}
			if (nowVisible)
			{
				MoveToEx(dc, currCol, viewRect.top,    NULL);
				LineTo  (dc, currCol, viewRect.bottom);
			}
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

void FocusCaret::Reverse(HDC dc, bool newVisibleState)
{
	DBG_START("FocusCaret", "Reverse", MG_DEBUG_CARET);

#if defined(MG_DEBUG)
	if	(	m_StartPoint.x != m_EndPoint.x
		&&	m_StartPoint.y != m_EndPoint.y)
	{
		GRect rect(m_StartPoint, m_EndPoint);
		DrawFocusRect(dc, &rect);
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

#include "DcHandle.h"

void LineCaret::Reverse(HDC dc, bool newVisibleState)
{
	GdiHandle        <HPEN> pen( CreatePen(PS_SOLID, 2,  DmsColor2COLORREF(m_LineColor) ) );
	GdiObjectSelector<HPEN> penSelector(dc, pen); 

	MoveToEx(dc, m_StartPoint.x, m_StartPoint.y, NULL);
	LineTo  (dc, m_EndPoint.x,   m_EndPoint.y);
}

//----------------------------------------------------------------------
// class  : PolygonCaret
//----------------------------------------------------------------------

void PolygonCaret::Reverse(HDC dc, bool newVisibleState)
{
	GdiHandle<HPEN>           invisiblePen(CreatePen(PS_NULL, 0, RGB(0, 0, 0)));
	GdiObjectSelector<HPEN>   penSelector(dc, invisiblePen);

	GdiHandle<HBRUSH>         brush( CreateSolidBrush(m_FillColor) );
	GdiObjectSelector<HBRUSH> brushSelector(dc, brush);

	CheckedGdiCall(
		Polygon(
			dc,
			m_First,
			m_Count
		)
	,	"DrawPolygon"
	);
}

//----------------------------------------------------------------------
// class  :	InvertRgnCaret
//----------------------------------------------------------------------
#include "Region.h"

// override AbstrCaret interface
void InvertRgnCaret::Reverse(HDC dc, bool newVisibleState)
{
	DBG_START("InvertRgnCaret", "Reverse", MG_DEBUG_CARET);

	Region rgn;
	GetRgn(rgn, dc);
	InvertRgn(dc, rgn.GetHandle());
}

void InvertRgnCaret::Move(const AbstrCaretOperator& caret_operator, HDC dc)
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
		InvertRgn(dc, orgRegion.GetHandle());
}

//----------------------------------------------------------------------
// class  : RectCaret
//----------------------------------------------------------------------

// override InvertRgnCaret interface
void RectCaret::GetRgn(Region& rgn, HDC dc) const
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

MovableRectCaret::MovableRectCaret(const TRect& objRect)
	:	m_ObjRect(objRect)
{
	if (m_ObjRect.Width() > 4 && m_ObjRect.Height() > 4)
		m_SubRect = TRect(m_ObjRect.Left()+2, m_ObjRect.Top()+2, m_ObjRect.Right()-2, m_ObjRect.Bottom()-2);
}

void MovableRectCaret::GetRgn(Region& rgn, HDC dc) const
{
	if (m_StartPoint != m_EndPoint)
	{
		TPoint diff = TPoint(m_EndPoint) - TPoint(m_StartPoint);
		rgn = Region(dc, m_ObjRect + diff );
		if (!m_SubRect.empty())
			rgn ^= Region(dc, m_SubRect + diff );
	}
}

//----------------------------------------------------------------------
// class  : BoundaryCaret
//----------------------------------------------------------------------

#include "MovableObject.h"

BoundaryCaret::BoundaryCaret(MovableObject* obj)
	:	MovableRectCaret(obj->GetCurrClientAbsRect())
{}

//----------------------------------------------------------------------
// class  : RoiCaret
//----------------------------------------------------------------------

#include "geo/Conversions.h"

#include "ViewPort.h"

// override InvertRgnCaret interface
void RoiCaret::GetRgn(Region& rgn, HDC dc) const
{
	RectCaret::GetRgn(rgn, dc);

	if (m_StartPoint != m_EndPoint)
	{
		const ViewPort* view = debug_cast<const ViewPort*>(m_UsedObject);
		CrdRect vp  = CrdRect(CrdPoint(0, 0), Convert<CrdPoint>(view->GetCurrClientSize()));
		CrdRect dvp = 
			CrdTransformation(
				Convert<CrdRect>(GRect(m_StartPoint, m_EndPoint)), 
				vp, OrientationType::Default
			).Reverse(vp);

		SRect viewPortRect = Inflate(Convert<SRect>(dvp),SPoint(1, 1));

		rgn ^= Region(Convert<GRect>(viewPortRect) );
	}
}

//----------------------------------------------------------------------
// class  : CircleCaret
//----------------------------------------------------------------------

CrdType CircleCaret::Radius() const
{
	return sqrt( 
		SqrDist<CrdType>(
			Convert<CrdPoint>(m_EndPoint  ), 
			Convert<CrdPoint>(m_StartPoint)
		)
	);
}

// override InvertRgnCaret interface
void CircleCaret::GetRgn(Region& rgn, HDC dc) const
{
	UInt32 radius = Radius();

	rgn = 
		Region(
			CreateEllipticRgn(
				m_StartPoint.x - radius, m_StartPoint.y - radius,
				m_StartPoint.x + radius, m_StartPoint.y + radius
			)
		);
}


