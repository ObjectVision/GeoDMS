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
	,	m_Cursor(nullptr)
{}

MovableObject::MovableObject(const MovableObject& src)
	:	GraphicObject(0)
	,	m_ClientLogicalSize(src.m_ClientLogicalSize)
	,	m_Cursor(nullptr)
{}

void MovableObject::SetBorder(bool hasBorder)
{
	if (hasBorder == HasBorder())
		return;
	auto owner = GetOwner().lock();
	if (hasBorder)
	{
		TPoint bottRight = m_ClientLogicalSize + m_RelPos;
		if (owner)
		{
			owner->GrowHor(BORDERSIZE*2, bottRight.X(), this);
			owner->GrowVer(BORDERSIZE*2, bottRight.Y(), this);
		}
		MoveTo(m_RelPos + Point<TType>(  BORDERSIZE,   BORDERSIZE));

		assert(m_ClientLogicalSize.X() >= 0);
		assert(m_ClientLogicalSize.Y() >= 0);
		m_State.Set(MOF_HasBorder);
	}
	else
	{
		m_State.Clear(MOF_HasBorder);
		MoveTo(m_RelPos - Point<TType>(  BORDERSIZE,   BORDERSIZE));
		TPoint bottRight = m_ClientLogicalSize + m_RelPos + Point<TType>(2*BORDERSIZE, 2*BORDERSIZE);
		if (owner)
		{
			owner->GrowHor(-BORDERSIZE*2, bottRight.X(), this);
			owner->GrowVer(-BORDERSIZE*2, bottRight.Y(), this);
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
	auto extents = GetCurrFullAbsLogicalRect();
	auto gExtents =  TRect2GRect(extents, GetScaleFactors());
	auto dv = GetDataView().lock(); if (!dv) return;
	DirectDC dc(dv.get(), Region(gExtents));
	if (!dc.IsEmpty())
		DrawBorder(dc, gExtents, revBorder);
}

void MovableObject::SetIsVisible(bool value)
{
	if (value == IsVisible())
		return;

	TRect relFullRect = GetCurrFullRelLogicalRect();

	auto owner = GetOwner().lock();
	if (value && owner)
	{
		owner->GrowHor(relFullRect.Size().X(), relFullRect.Left(), this);
		owner->GrowVer(relFullRect.Size().Y(), relFullRect.Top (), this);
	}

	base_type::SetIsVisible(value);

	if (!value && owner)
	{
		owner->GrowHor(-relFullRect.Size().X(), relFullRect.Right (),  this);
		owner->GrowVer(-relFullRect.Size().Y(), relFullRect.Bottom(), this);
	}
}

void MovableObject::SetDisconnected()
{
	auto dv = GetDataView().lock(); 
	if (dv && IsActive())
		dv->Activate(GetOwner().lock().get());
	base_type::SetDisconnected();
}

TRect MovableObject::GetBorderLogicalExtents() const
{
	return (HasBorder())
		?	TRect(Point<TType>(-BORDERSIZE, -BORDERSIZE), Point<TType>(BORDERSIZE, BORDERSIZE))
		:	TRect(Point<TType>(0, 0), Point<TType>(0, 0));
}

TPoint MovableObject::GetBorderLogicalSize() const
{
	return (HasBorder())
		? Point<TType>(2*BORDERSIZE, 2*BORDERSIZE)
		: Point<TType>(0, 0);
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
			TPoint logDelta = newClientRelPos - m_RelPos;
			GPoint devDelta = TPoint2GPoint(logDelta, GetScaleFactors());
			GRect  clipRect = GetParentClipAbsRect();
			GRect  scrollRect = clipRect;
			scrollRect &= GetCurrFullAbsDeviceRect();
			if (!scrollRect.empty())
			{
				auto dv = GetDataView().lock(); if (!dv) return;
				dv->ScrollDevice(devDelta, scrollRect, clipRect, this);
				TranslateDrawnRect(clipRect, devDelta);
			}
		}
		else
			InvalidateDraw();
	}

	m_RelPos = newClientRelPos;

	auto owner = GetOwner().lock();
	if (!IsDrawn() && owner)
		owner->InvalidateClientRect(GetCurrFullRelLogicalRect() );

	MG_DEBUGCODE( CheckSubStates(); )
}

GRect MovableObject::GetDrawnClientAbsDeviceRect() const
{
	assert(IsDrawn());
	return m_DrawnFullAbsRect & GetCurrClientAbsDeviceRect();
}

GRect MovableObject::GetDrawnNettAbsDeviceRect() const
{
	assert(IsDrawn());
	return m_DrawnFullAbsRect & TRect2GRect(GetCurrNettAbsLogicalRect(), GetScaleFactors());
}

GRect MovableObject::GetParentClipAbsRect() const
{
	auto owner = GetOwner().lock();
	if (owner)
		return owner->GetDrawnNettAbsDeviceRect();
	auto dv = GetDataView().lock(); if (!dv) return GRect();
	return dv->ViewDeviceRect();
}


void MovableObject::SetClientSize(TPoint newSize)
{
	assert(newSize.X() >= 0);
	assert(newSize.Y() >= 0);

	if (m_ClientLogicalSize == newSize)
		return;

//	InvalidateDraw();
	GrowHor(newSize.X()  - m_ClientLogicalSize.X(), m_ClientLogicalSize.X());
	GrowVer(newSize.Y()  - m_ClientLogicalSize.Y(), m_ClientLogicalSize.Y());

	assert(m_ClientLogicalSize == newSize);
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
	if (!r.empty())
		SetClientRect(r);
}

void MovableObject::InvalidateClientRect(TRect rect) const
{
	if (!IsDrawn())
		return;

	GRect gRect = TRect2GRect( rect + GetCurrClientAbsLogicalPos(), GetScaleFactors() );
	gRect &= GetDrawnNettAbsDeviceRect();
	if (gRect.empty())
		return;

	auto dv = GetDataView().lock(); if (!dv) return;
	dv->InvalidateDeviceRect( gRect );
}

TPoint MovableObject::CalcClientSize() const
{
	UpdateView();
	return m_ClientLogicalSize;
}

TPoint MovableObject::CalcMaxSize() const
{
	return CalcClientSize() + GetBorderLogicalExtents().Size();
}

TPoint MovableObject::GetCurrClientAbsLogicalPos() const
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

TPoint MovableObject::GetCurrClientAbsLogicalPos(const GraphVisitor& v) const
{
	return m_RelPos + v.GetClientLogicalOffset();
/*
	TPoint result = m_RelPos;
	auto owner = GetOwner().lock();
	while (owner)
	{
		result += owner->m_RelPos;
		owner = owner->GetOwner().lock();
	}
	return result;
*/
}

GPoint MovableObject::GetCurrClientAbsDevicePos(const GraphVisitor& v) const
{ 
	return TPoint2GPoint(GetCurrClientAbsLogicalPos(v), v.GetSubPixelFactors()); 
}

TRect  MovableObject::GetCurrClientAbsLogicalRect(const GraphVisitor& v) const 
{
	TPoint pos = GetCurrClientAbsLogicalPos(v); return TRect(pos, pos + m_ClientLogicalSize);
}

GRect  MovableObject::GetCurrClientAbsDeviceRect(const GraphVisitor& v) const 
{
	return TRect2GRect(GetCurrClientAbsLogicalRect(v), v.GetSubPixelFactors());
}

/*
//REMOVE
TRect MovableObject::CalcFullAbsLogicalRect(const GraphVisitor& v) const
{
	return CalcFullRelRect() + v.GetClientLogicalOffset();
}
*/

TRect MovableObject::GetCurrNettAbsLogicalRect(const GraphVisitor& v) const
{
	return GetCurrNettRelLogicalRect() + v.GetClientLogicalOffset();
}

GRect MovableObject::GetCurrFullAbsDeviceRect(const GraphVisitor& v) const 
{ 
	return TRect2GRect(GetCurrFullAbsLogicalRect(v), v.GetSubPixelFactors()); 
}

TRect MovableObject::GetCurrNettAbsLogicalRect() const
{
	return GetCurrNettRelLogicalRect() + TPoint(GetCurrClientAbsLogicalPos() - m_RelPos);
}

GraphVisitState MovableObject::InviteGraphVistor(AbstrVisitor& v)
{
	return v.DoMovable(this);
}


HBITMAP MovableObject::GetAsDDBitmap(DataView* dv, CrdType subPixelFactor, MovableObject* extraObj)
{
	GPoint size = TPoint2GPoint(CalcClientSize(), GetScaleFactors());
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
	GraphDrawer drawer(memDC, rgn, dv, GdMode(GD_DrawBackground|GD_UpdateData|GD_DrawData), DPoint(subPixelFactor, subPixelFactor));

	AddClientLogicalOffset useZeroBase(&drawer, -m_RelPos);
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

ControlRegion MovableObject::GetControlDeviceRegion(GType absX) const
{
	TRect currAbsRect = GetCurrClientAbsLogicalRect();
	TType logicalX = absX / GetScaleFactors().first;
	auto margin = 4;
	if (logicalX >= currAbsRect.Right() - margin)
		return RG_RIGHT;
	else if (logicalX <= currAbsRect.Left() + margin)
		return RG_LEFT;
	return RG_MIDDLE;

}

void MovableObject::GrowHor(TType deltaX, TType relPosX, const MovableObject* sourceItem)
{
	auto dv = GetDataView().lock();
	auto owner = GetOwner().lock();

	if(sourceItem)
		return;

	assert(relPosX <= m_ClientLogicalSize.X());

	if (!deltaX)
		return;

	assert(!IsDrawn() || IsIncluding(TRect2GRect(GetCurrFullAbsLogicalRect(), GetScaleFactors()), GetDrawnFullAbsDeviceRect() ));

	TType  oldFullRelRight  = GetCurrFullRelLogicalRect().Right();
	TPoint oldClientSize    = GetCurrClientSize();

	assert(relPosX >= 0 );
	assert(relPosX + GetCurrClientRelPos().X() <= oldFullRelRight);
	assert(!IsDrawn() || IsIncluding(TRect2GRect(GetCurrFullAbsLogicalRect(), GetScaleFactors()), GetDrawnFullAbsDeviceRect() ));

	if (deltaX > 0)
	{
		if (owner && IsVisible()) 
			owner->GrowHor(deltaX, oldFullRelRight, this);
		m_ClientLogicalSize.X() += deltaX;
	}

	if (dv && dv->GetHWnd() && (!owner || owner->IsDrawn()) && IsVisible())
	{
		auto sf = GetScaleFactors();

		TRect oldAbsRect = TRect(shp2dms_order<TType>(relPosX, 0), oldClientSize) + GetCurrClientAbsLogicalPos();
		GRect clipRect   = GetParentClipAbsRect();
		if (IsDrawn())
		{
			if (IsChildCovered() || !IsTransparent())
			{
				TRect be = GetBorderLogicalExtents(); be.Left() = 0;
				oldAbsRect += TRect(be);
				assert(oldAbsRect.Left() <= oldAbsRect.Right());
				assert(IsIncluding(clipRect, GetDrawnFullAbsDeviceRect())); // invariant IsIncluding relation

				TPoint delta = shp2dms_order<TType>(deltaX, 0);

				GRect oldVisibleAbsRect = TRect2GRect(oldAbsRect, sf) & GetDrawnFullAbsDeviceRect();
				assert(IsIncluding(clipRect, oldVisibleAbsRect)); // follows from previous assertion and clipping

				if (!oldVisibleAbsRect.empty())
					dv->ScrollDevice(TPoint2GPoint(delta, sf), oldVisibleAbsRect, clipRect, this);
				ResizeDrawnRect(clipRect, TPoint2GPoint(delta, sf), GPoint(TType2GType(oldAbsRect.Left()*sf.first), MaxValue<GType>()));
				assert(!IsDrawn() || IsIncluding(clipRect, GetDrawnFullAbsDeviceRect())); // invariant IsIncluding relation

				// DEBUG
				if (deltaX < 0)
					oldAbsRect.Left() = oldAbsRect.Right() + deltaX;
				else
					oldAbsRect.Right() = oldAbsRect.Left() + deltaX;
				oldAbsRect &= Convert<TRect>(dv->Reverse(clipRect));
				if (!oldAbsRect.empty())
					dv->InvalidateDeviceRect(TRect2GRect(oldAbsRect, sf));
			}
			else
				InvalidateDraw();
		}
		if (!IsDrawn() && deltaX > 0)
		{
			oldAbsRect.Left () = oldAbsRect.Right();
			oldAbsRect.Right() = oldAbsRect.Left() + deltaX;
			clipRect &= TRect2GRect(oldAbsRect, sf);
			if (!clipRect.empty())
				dv->InvalidateDeviceRect(clipRect);
		}
	}

	if (deltaX < 0)
		m_ClientLogicalSize.X() += deltaX;

	assert(!IsDrawn() || IsIncluding(TRect2GRect(GetCurrFullAbsLogicalRect(), GetScaleFactors()), GetDrawnFullAbsDeviceRect() ));

	if (owner && IsVisible()) 
	{
		if (deltaX < 0)
			owner->GrowHor(deltaX, oldFullRelRight, this);
	}

	assert(!IsDrawn() || IsIncluding(TRect2GRect(GetCurrFullAbsLogicalRect(), GetScaleFactors()), GetDrawnFullAbsDeviceRect() ));
}

void MovableObject::GrowVer(TType deltaY, TType relPosY, const MovableObject* sourceItem)
{
	auto dv = GetDataView().lock();
	auto owner = GetOwner().lock();

	if(sourceItem)
		return;

	assert(relPosY <= m_ClientLogicalSize.Y());

	if (!deltaY)
		return;

	assert(!IsDrawn() || IsIncluding(TRect2GRect(GetCurrFullAbsLogicalRect(), GetScaleFactors()), GetDrawnFullAbsDeviceRect() ));

	TType  oldFullRelBottom = GetCurrFullRelLogicalRect().Bottom();
	TPoint oldClientSize    = GetCurrClientSize();

	assert(relPosY >= 0 );
	assert(relPosY + GetCurrClientRelPos().Y() <= oldFullRelBottom);
	assert(!IsDrawn() || IsIncluding(TRect2GRect(GetCurrFullAbsLogicalRect(), GetScaleFactors()), GetDrawnFullAbsDeviceRect() ));

	if (deltaY > 0)
	{
		if (owner && IsVisible()) 
			owner->GrowVer(deltaY, oldFullRelBottom, this);
		m_ClientLogicalSize.Y() += deltaY;
	}


	if (dv && dv->GetHWnd() && ( !owner || owner->IsDrawn()) && IsVisible())
	{
		auto sf = GetScaleFactors();

		TRect oldAbsRect = TRect(shp2dms_order<TType>(0, relPosY), oldClientSize) + GetCurrClientAbsLogicalPos();
		GRect clipRect   = GetParentClipAbsRect();
		if (IsDrawn())
		{
			if (IsChildCovered() || !IsTransparent())
			{
				TRect be = GetBorderLogicalExtents(); be.Top() = 0;
				oldAbsRect += TRect(be);
				assert(oldAbsRect.Top() <= oldAbsRect.Bottom());
				assert(IsIncluding(clipRect, GetDrawnFullAbsDeviceRect())); // invariant IsIncluding relation

				TPoint delta = shp2dms_order<TType>(0, deltaY);

				GRect oldVisibleAbsRect = TRect2GRect(oldAbsRect, sf) & GetDrawnFullAbsDeviceRect();
				assert(IsIncluding(clipRect, oldVisibleAbsRect)); // follows from previous assertion and clipping

				if (!oldVisibleAbsRect.empty())
					dv->ScrollDevice(TPoint2GPoint(delta, GetScaleFactors()), oldVisibleAbsRect, clipRect, this);
				ResizeDrawnRect(clipRect, TPoint2GPoint(delta, GetScaleFactors()), GPoint(MaxValue<GType>(), TType2GType(oldAbsRect.Top() * sf.second)));
				assert(!IsDrawn() || IsIncluding(clipRect, GetDrawnFullAbsDeviceRect())); // invariant IsIncluding relation

				// DEBUG
				if (deltaY < 0)
					oldAbsRect.Top()    = oldAbsRect.Bottom() + deltaY;
				else
					oldAbsRect.Bottom() = oldAbsRect.Top()    + deltaY;
				oldAbsRect &= Convert<TRect>(dv->Reverse(clipRect));
				if (!oldAbsRect.empty())
					dv->InvalidateDeviceRect(TRect2GRect(oldAbsRect, sf));
			}
			else
				InvalidateDraw();
		}
		if (!IsDrawn() && deltaY > 0)
		{
			oldAbsRect.Top ()   = oldAbsRect.Bottom();
			oldAbsRect.Bottom() = oldAbsRect.Top()     + deltaY;
			oldAbsRect &= Convert<TRect>(dv->Reverse(clipRect));
			if (!oldAbsRect.empty())
				dv->InvalidateDeviceRect(TRect2GRect(oldAbsRect, sf));
		}

	}

	if (deltaY < 0)
		m_ClientLogicalSize.Y() += deltaY;

	assert(!IsDrawn() || IsIncluding(TRect2GRect(GetCurrFullAbsLogicalRect(), GetScaleFactors()), GetDrawnFullAbsDeviceRect() ));

	if (owner && IsVisible())
	{
		if (deltaY < 0) 
			owner->GrowVer(deltaY, oldFullRelBottom, this);
	}

	assert(!IsDrawn() || IsIncluding(TRect2GRect(GetCurrFullAbsLogicalRect(), GetScaleFactors()), GetDrawnFullAbsDeviceRect() ));
}

#if defined(MG_DEBUG)
void MovableObject::CheckState() const
{
	base_type::CheckState();
	if (!IsDrawn())
		return;
	auto sf = GetScaleFactors();
	assert(IsIncluding(TRect2GRect(GetCurrFullAbsLogicalRect(), sf), GetDrawnFullAbsDeviceRect() ) );
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

