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

#include "MovableObject.h"

#include "act/TriggerOperator.h"
#include "dbg/DebugContext.h"
#include "ser/AsString.h"
#include "utl/IncrementalLock.h"
#include "utl/mySPrintF.h"

#include "ClipBoard.h"
#include "DataView.h"
#include "MouseEventDispatcher.h"
#include "GraphVisitor.h"
#include "ScrollPort.h"


//----------------------------------------------------------------------
// class  : MovableObject
//----------------------------------------------------------------------

MovableObject::MovableObject(MovableObject* owner)
	:	GraphicObject(owner)
	,	m_RelPos(0, 0)
	,	m_ClientSize(0, 0)
	,	m_Cursor(nullptr)
{
}

MovableObject::MovableObject(const MovableObject& src)
	:	GraphicObject(0)
	,	m_RelPos(0, 0)
	,	m_ClientSize(src.m_ClientSize)
	,	m_Cursor(nullptr)
{
}

void MovableObject::SetBorder(bool hasBorder)
{
	if (hasBorder == HasBorder())
		return;
	auto owner = GetOwner().lock();
	if (hasBorder)
	{
		TPoint bottRight = m_ClientSize + m_RelPos;
		if (owner)
		{
			owner->GrowHor(BORDERSIZE*2, bottRight.x(), this);
			owner->GrowVer(BORDERSIZE*2, bottRight.y(), this);
		}
		MoveTo(m_RelPos + TPoint(  BORDERSIZE,   BORDERSIZE));

		dms_assert(m_ClientSize.x() >= 0);
		dms_assert(m_ClientSize.y() >= 0);
		m_State.Set(MOF_HasBorder);
	}
	else
	{
		m_State.Clear(MOF_HasBorder);
		MoveTo(m_RelPos - TPoint(  BORDERSIZE,   BORDERSIZE));
		TPoint bottRight = m_ClientSize + m_RelPos + TPoint(2*BORDERSIZE, 2*BORDERSIZE);
		if (owner)
		{
			owner->GrowHor(-BORDERSIZE*2, bottRight.x(), this);
			owner->GrowVer(-BORDERSIZE*2, bottRight.y(), this);
		}
	}
}

void MovableObject::SetRevBorder(bool revBorder)
{
	if (revBorder == RevBorder())
		return;
	m_State.Set(MOF_RevBorder, revBorder);
	if (!HasBorder() || !IsDrawn())
		return;
	GRect rect = TRect2GRect(GetCurrFullAbsRect());
	auto dv = GetDataView().lock(); if (!dv) return;
	DirectDC dc(dv.get(), Region(rect));
	if (!dc.IsEmpty())
		DrawBorder(dc, rect, revBorder);
}

void MovableObject::SetIsVisible(bool value)
{
	if (value == IsVisible())
		return;

	TRect relFullRect = GetCurrFullRelRect();

	auto owner = GetOwner().lock();
	if (value && owner)
	{
		owner->GrowHor(relFullRect.Size().x(), relFullRect.Left(), this);
		owner->GrowVer(relFullRect.Size().y(), relFullRect.Top (), this);
	}

	base_type::SetIsVisible(value);

	if (!value && owner)
	{
		owner->GrowHor(-relFullRect.Size().x(), relFullRect.Right (),  this);
		owner->GrowVer(-relFullRect.Size().y(), relFullRect.Bottom(), this);
	}
}

void MovableObject::SetDisconnected()
{
	auto dv = GetDataView().lock(); 
	if (dv && IsActive())
		dv->Activate(GetOwner().lock().get());
	base_type::SetDisconnected();
}

GRect MovableObject::GetBorderPixelExtents() const
{
	return (HasBorder())
		?	GRect(-BORDERSIZE, -BORDERSIZE, BORDERSIZE, BORDERSIZE)
		:	GRect(0, 0, 0, 0);
}

GPoint MovableObject::GetBorderPixelSize   () const
{
	return (HasBorder())
		?	GPoint(2*BORDERSIZE, 2*BORDERSIZE)
		:	GPoint(0, 0);
}

void MovableObject::MoveTo(TPoint newClientRelPos) // SetClientRelPos
{
	if (newClientRelPos == m_RelPos)
		return;

	if (IsDrawn())
	{
		if (IsChildCovered() || !IsTransparent())
		{
//			GRect  oldAbsFullRect = TRect2GRect(GetCurrFullAbsRect());
			GPoint delta    = TPoint2GPoint(newClientRelPos - m_RelPos);
			GRect  clipRect = GetParentClipAbsRect();
			GRect  scrollRect = clipRect;
			scrollRect &= GetCurrFullAbsRect();
			if (!scrollRect.empty())
			{
				auto dv = GetDataView().lock(); if (!dv) return;
				dv->Scroll(delta, scrollRect, clipRect, this);
				TranslateDrawnRect(clipRect, delta);
			}
		}
		else
			InvalidateDraw();
	}

	m_RelPos = newClientRelPos;

	auto owner = GetOwner().lock();
	if (!IsDrawn() && owner)
		owner->InvalidateClientRect(GetCurrFullRelRect() );

	MG_DEBUGCODE( CheckSubStates(); )
}

GRect MovableObject::GetDrawnClientAbsRect() const
{
	dms_assert(IsDrawn());
	return m_DrawnFullAbsRect & TRect2GRect(GetCurrClientAbsRect());
}

GRect MovableObject::GetDrawnNettAbsRect() const
{
	dms_assert(IsDrawn());
	return m_DrawnFullAbsRect & TRect2GRect(GetCurrNettAbsRect());
}

GRect MovableObject::GetParentClipAbsRect() const
{
	auto owner = GetOwner().lock();
	if (owner)
		return owner->GetDrawnNettAbsRect();
	auto dv = GetDataView().lock(); if (!dv) return GRect();
	return dv->ViewRect();
}


void MovableObject::SetClientSize(TPoint newSize)
{
	dms_assert(newSize.x() >= 0);
	dms_assert(newSize.y() >= 0);

	if (m_ClientSize == newSize)
		return;

//	InvalidateDraw();
	GrowHor(newSize.x()  - m_ClientSize.x(), m_ClientSize.x(), 0 );
	GrowVer(newSize.y()  - m_ClientSize.y(), m_ClientSize.y(), 0 );

	dms_assert(m_ClientSize == newSize);
}

void MovableObject::SetClientRect(const TRect& r)
{
	SetClientSize(LowerBound(GetCurrClientSize(), r.Size()));
	MoveTo(r.TopLeft()); 
	SetClientSize(r.Size()); 
}

void MovableObject::SetFullRelRect(TRect r)
{
	if (HasBorder())
		r.Shrink(BORDERSIZE);
	SetClientRect(r);
}

void MovableObject::InvalidateClientRect(TRect rect) const
{
	if (!IsDrawn())
		return;

	GRect gRect = TRect2GRect( rect + GetCurrClientAbsPos() );
	gRect &= GetDrawnNettAbsRect();
	if (gRect.empty())
		return;

	auto dv = GetDataView().lock(); if (!dv) return;
	dv->InvalidateRect( gRect  );
}

TPoint MovableObject::CalcClientSize() const
{
	UpdateView();
	return m_ClientSize;
}

TPoint MovableObject::CalcMaxSize() const
{
	return CalcClientSize() + TPoint(GetBorderPixelExtents().Size());
}

TPoint MovableObject::GetCurrClientAbsPos() const
{
	TPoint result = m_RelPos;
	auto owner = GetOwner().lock();
	while (owner)
	{
		result += owner->m_RelPos;
		owner = owner->GetOwner().lock();
	}
	return result;
}

TRect MovableObject::CalcFullAbsRect(const GraphVisitor& v) const
{
	return CalcFullRelRect() + v.GetClientOffset();
}

TRect MovableObject::GetCurrFullAbsRect(const GraphVisitor& v) const
{
	return GetCurrFullRelRect() + v.GetClientOffset();
}

TRect MovableObject::GetCurrNettAbsRect(const GraphVisitor& v) const
{
	return GetCurrNettRelRect() + v.GetClientOffset();
}

GraphVisitState MovableObject::InviteGraphVistor(AbstrVisitor& v)
{
	return v.DoMovable(this);
}


HBITMAP MovableObject::GetAsDDBitmap(DataView* dv, CrdType subPixelFactor, MovableObject* extraObj)
{
	GPoint size = TPoint2GPoint(CalcClientSize());
	SharedStrContextHandle context(mySSPrintF("while Copying %d x % u pixels to a Device Dependent Bitmap (DDB)", size.x, size.y));

	if (size.x >= 0x8000 || size.y >= 0x8000)
		throwErrorF("GDI Error", "FullExtents %s of this Graphic are above the GeoDms limit of 0x8000 rows and 0x8000 colums; consider tiling", 
			AsString(size).c_str()
		);

	GdiHandle<HBITMAP> hBitmap(
		CreateCompatibleBitmap(
			DcHandleBase(dv ? dv->GetHWnd() : NULL), // memDC,
			size.x, size.y
		)
	);

	CompatibleDcHandle memDC(NULL, dv ? dv->GetDefaultFont(FontSizeCategory::SMALL, subPixelFactor) : NULL);

	GdiObjectSelector<HBITMAP> selectBitmap(memDC, hBitmap);

	Region rgn(size);

	SuspendTrigger::FencedBlocker xxx;
	GraphDrawer drawer(memDC, rgn, dv, GdMode(GD_DrawBackground|GD_UpdateData|GD_DrawData), subPixelFactor); 

	AddClientOffset useZeroBase(&drawer, -m_RelPos);
	bool suspended = drawer.Visit(this); //DrawBackgroud && DrawData
	dms_assert(!suspended); 
	if (extraObj)
	{
		suspended = drawer.Visit(extraObj);
		dms_assert(!suspended); 
	}

	return hBitmap.release();
}

void GetDIBitsWithBmp(BITMAPINFO& bmp, GPoint size, UInt32 bitCount, HDC hDc, HBITMAP hDDBitmap, void* pvBits)
{
	bmp.bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
	bmp.bmiHeader.biWidth         =  size.x;
	bmp.bmiHeader.biHeight        = -size.y; // negative for a top to bottom bitmap
	bmp.bmiHeader.biPlanes        = 1;
	bmp.bmiHeader.biBitCount      = bitCount;
	bmp.bmiHeader.biCompression   = BI_RGB;
	bmp.bmiHeader.biSizeImage     = 0;
	bmp.bmiHeader.biXPelsPerMeter = 0;
	bmp.bmiHeader.biYPelsPerMeter = 0;
	bmp.bmiHeader.biClrUsed       = (bitCount <= 16) ? (1 << bitCount) : 0;
	bmp.bmiHeader.biClrImportant  = bmp.bmiHeader.biClrUsed;

	int result = GetDIBits(
		hDc,
		hDDBitmap,
		0, size.y,
		pvBits,
		&bmp,
		DIB_RGB_COLORS
	);
	CheckedGdiCall(result, "GetDIBits");

	dms_assert(pvBits);
}

void MovableObject::CopyToClipboard(DataView* dv)
{
	ClipBoard clipBoard(false);
	if (!clipBoard.IsOpen())
		throwItemError("Cannot open Clipboard");

	GdiHandle<HBITMAP> hBmp(GetAsDDBitmap(dv) );
	clipBoard.SetBitmap(hBmp);
}

ControlRegion MovableObject::GetControlRegion(TType absX) const
{
	TRect currAbsRect = GetCurrClientAbsRect();
	if (absX >= currAbsRect.Right()-4)
		return RG_RIGHT;
	else if (absX <= currAbsRect.Left()+4)
		return RG_LEFT;
	return RG_MIDDLE;

}

void MovableObject::GrowHor(TType deltaX, TType relPosX, const MovableObject* sourceItem)
{
	auto dv = GetDataView().lock();
	auto owner = GetOwner().lock();

//	MakeMin(relPosX, m_ClientSize.x);

	if(sourceItem)
		return;

	dms_assert(relPosX <= m_ClientSize.x());

//	dms_assert(GetOwner()); // Grow stops at ScrollPort, but not when resizing a viewControl
	if (!deltaX)
		return;

	dms_assert(!IsDrawn() || IsIncluding(TRect2GRect(GetCurrFullAbsRect()), GetDrawnFullAbsRect() ));

	TType  oldFullRelRight  = GetCurrFullRelRect().Right();
	TPoint oldClientSize    = GetCurrClientSize();

	dms_assert(relPosX >= 0 );
	dms_assert(relPosX + GetCurrClientRelPos().x() <= oldFullRelRight);
	dms_assert(!IsDrawn() || IsIncluding(TRect2GRect(GetCurrFullAbsRect()), GetDrawnFullAbsRect() ));

	if (deltaX > 0)
	{
		if (owner && IsVisible()) 
			owner->GrowHor(deltaX, oldFullRelRight, this);
		m_ClientSize.x() += deltaX;
	}

	if (dv && (!owner || owner->IsDrawn()) && IsVisible())
	{
		TRect oldAbsRect = TRect(TPoint(relPosX, 0), oldClientSize) + GetCurrClientAbsPos();
		GRect clipRect   = GetParentClipAbsRect();
		if (IsDrawn())
		{
			if (IsChildCovered() || !IsTransparent())
			{
				GRect be = GetBorderPixelExtents(); be.left = 0;
				oldAbsRect += TRect(be);
				dms_assert(oldAbsRect.Left() <= oldAbsRect.Right());
				dms_assert(IsIncluding(clipRect, GetDrawnFullAbsRect())); // invariant IsIncluding relation

				TPoint delta(deltaX, 0);

				GRect oldVisibleAbsRect = TRect2GRect(oldAbsRect) & GetDrawnFullAbsRect();
				dms_assert(IsIncluding(clipRect, oldVisibleAbsRect)); // follows from previous assertion and clipping

				if (!oldVisibleAbsRect.empty())
					dv->Scroll(TPoint2GPoint(delta), oldVisibleAbsRect, clipRect, this);
				ResizeDrawnRect(clipRect, TPoint2GPoint(delta), GPoint(TType2GType(oldAbsRect.Left()), MaxValue<GType>()));
				dms_assert(!IsDrawn() || IsIncluding(clipRect, GetDrawnFullAbsRect())); // invariant IsIncluding relation

				// DEBUG
				if (deltaX < 0)
					oldAbsRect.Left() = oldAbsRect.Right() + deltaX;
				else
					oldAbsRect.Right() = oldAbsRect.Left() + deltaX;
				oldAbsRect &= TRect(clipRect);
				if (!oldAbsRect.empty())
					dv->InvalidateRect(TRect2GRect(oldAbsRect));
			}
			else
				InvalidateDraw();
		}
		if (!IsDrawn() && deltaX > 0)
		{
			oldAbsRect.Left () = oldAbsRect.Right();
			oldAbsRect.Right() = oldAbsRect.Left() + deltaX;
			clipRect &= TRect2GRect(oldAbsRect);
			if (!clipRect.empty())
				dv->InvalidateRect(clipRect);
		}
	}

	if (deltaX < 0)
		m_ClientSize.x() += deltaX;

	dms_assert(!IsDrawn() || IsIncluding(TRect2GRect(GetCurrFullAbsRect()), GetDrawnFullAbsRect() ));

	if (owner && IsVisible()) 
	{
		if (deltaX < 0)
			owner->GrowHor(deltaX, oldFullRelRight, this);
//		else if (dynamic_cast<ScrollPort*>(GetOwner()))
//			GetOwner()->GrowHor(0, oldFullRelRight + deltaX, this);
	}

	dms_assert(!IsDrawn() || IsIncluding(TRect2GRect(GetCurrFullAbsRect()), GetDrawnFullAbsRect() ));
}

void MovableObject::GrowVer(TType deltaY, TType relPosY, const MovableObject* sourceItem)
{
	auto dv = GetDataView().lock();
	auto owner = GetOwner().lock();

//	MakeMin(relPosY, m_ClientSize.y);

	if(sourceItem)
		return;

	dms_assert(relPosY <= m_ClientSize.y());

//	dms_assert(GetOwner()); // Grow stops at ScrollPort, but not when resizing a viewControl
	if (!deltaY)
		return;

	dms_assert(!IsDrawn() || IsIncluding(TRect2GRect(GetCurrFullAbsRect()), GetDrawnFullAbsRect() ));

	TType  oldFullRelBottom = GetCurrFullRelRect().Bottom();
	TPoint oldClientSize    = GetCurrClientSize();

	dms_assert(relPosY >= 0 );
	dms_assert(relPosY + GetCurrClientRelPos().y() <= oldFullRelBottom);
	dms_assert(!IsDrawn() || IsIncluding(TRect2GRect(GetCurrFullAbsRect()), GetDrawnFullAbsRect() ));

	if (deltaY > 0)
	{
		if (owner && IsVisible()) 
			owner->GrowVer(deltaY, oldFullRelBottom, this);
		m_ClientSize.y() += deltaY;
	}


	if (dv && ( !owner || owner->IsDrawn()) && IsVisible())
	{
		TRect oldAbsRect = TRect(TPoint(0, relPosY), oldClientSize) + GetCurrClientAbsPos();
		GRect clipRect   = GetParentClipAbsRect();
		if (IsDrawn())
		{
			if (IsChildCovered() || !IsTransparent())
			{
				GRect be = GetBorderPixelExtents(); be.top = 0;
				oldAbsRect += TRect(be);
				dms_assert(oldAbsRect.Top() <= oldAbsRect.Bottom());
				dms_assert(IsIncluding(clipRect, GetDrawnFullAbsRect())); // invariant IsIncluding relation

				TPoint delta(0, deltaY);

				GRect oldVisibleAbsRect = TRect2GRect(oldAbsRect) & GetDrawnFullAbsRect();
				dms_assert(IsIncluding(clipRect, oldVisibleAbsRect)); // follows from previous assertion and clipping

				if (!oldVisibleAbsRect.empty())
					dv->Scroll(TPoint2GPoint(delta), oldVisibleAbsRect, clipRect, this);
				ResizeDrawnRect(clipRect, TPoint2GPoint(delta), GPoint(MaxValue<GType>(), TType2GType(oldAbsRect.Top())));
				dms_assert(!IsDrawn() || IsIncluding(clipRect, GetDrawnFullAbsRect())); // invariant IsIncluding relation

				// DEBUG
				if (deltaY < 0)
					oldAbsRect.Top()    = oldAbsRect.Bottom() + deltaY;
				else
					oldAbsRect.Bottom() = oldAbsRect.Top()    + deltaY;
				oldAbsRect &= TRect(clipRect);
				if (!oldAbsRect.empty())
					dv->InvalidateRect(TRect2GRect(oldAbsRect));
			}
			else
				InvalidateDraw();
		}
		if (!IsDrawn() && deltaY > 0)
		{
			oldAbsRect.Top ()   = oldAbsRect.Bottom();
			oldAbsRect.Bottom() = oldAbsRect.Top()     + deltaY;
			oldAbsRect &= TRect(clipRect);
			if (!oldAbsRect.empty())
				dv->InvalidateRect(TRect2GRect(oldAbsRect));
		}

	}

	if (deltaY < 0)
		m_ClientSize.y() += deltaY;

	dms_assert(!IsDrawn() || IsIncluding(TRect2GRect(GetCurrFullAbsRect()), GetDrawnFullAbsRect() ));

	if (owner && IsVisible())
	{
		if (deltaY < 0) 
			owner->GrowVer(deltaY, oldFullRelBottom, this);
//		else if (dynamic_cast<ScrollPort*>(GetOwner()))
//			GetOwner()->GrowVer(0, oldFullRelBottom + deltaY, this);
	}

	dms_assert(!IsDrawn() || IsIncluding(TRect2GRect(GetCurrFullAbsRect()), GetDrawnFullAbsRect() ));
}

#if defined(MG_DEBUG)
void MovableObject::CheckState() const
{
	base_type::CheckState();
	dms_assert(!IsDrawn() || IsIncluding(TRect2GRect(GetCurrFullAbsRect()), GetDrawnFullAbsRect() ) );
}
#endif

bool MovableObject::MouseEvent(MouseEventDispatcher& med)
{
	if (med.GetEventInfo().m_EventID & EID_SETCURSOR)
		return UpdateCursor();

	return base_type::MouseEvent(med);
}

HCURSOR MovableObject::SetViewPortCursor(HCURSOR hCursor)
{
	if (m_Cursor != hCursor)
	{
		std::swap(m_Cursor, hCursor);
		UpdateCursor();
	}
	return hCursor;
}

bool MovableObject::UpdateCursor() const
{
	if (m_Cursor)
	{
		SetCursor(m_Cursor);
		return true;
	}
	auto owner = m_Owner.lock();
	if (!owner)
		return false;
	return debug_cast<const MovableObject*>(owner.get())->UpdateCursor();
}

