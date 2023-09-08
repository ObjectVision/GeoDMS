// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////


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
		CrdPoint bottRight = m_RelPos + Convert<CrdPoint>(m_ClientLogicalSize);
		if (owner)
		{
			owner->GrowHor(BORDERSIZE*2, bottRight.X(), this);
			owner->GrowVer(BORDERSIZE*2, bottRight.Y(), this);
		}
		MoveTo(m_RelPos + Point<CrdType>(  BORDERSIZE,   BORDERSIZE));

		assert(m_ClientLogicalSize.X() >= 0);
		assert(m_ClientLogicalSize.Y() >= 0);
		m_State.Set(MOF_HasBorder);
	}
	else
	{
		m_State.Clear(MOF_HasBorder);
		MoveTo(m_RelPos - Point<CrdType>(  BORDERSIZE,   BORDERSIZE));
		auto bottRight = m_RelPos + Convert<CrdPoint>(m_ClientLogicalSize) + Point<CrdType>(2*BORDERSIZE, 2*BORDERSIZE);
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
	auto scaledExtents = ScaleCrdRect(extents, GetScaleFactors());
	auto gExtents = CrdRect2GRect(scaledExtents);
	auto dv = GetDataView().lock(); if (!dv) return;
	DirectDC dc(dv.get(), Region(gExtents));
	if (!dc.IsEmpty())
		DrawBorder(dc, gExtents, revBorder);
}

void MovableObject::SetIsVisible(bool value)
{
	if (value == IsVisible())
		return;

	auto relFullRect = GetCurrFullRelLogicalRect();

	auto owner = GetOwner().lock();
	if (value && owner)
	{
		owner->GrowHor(Width (relFullRect), relFullRect.first.X(), this);
		owner->GrowVer(Height(relFullRect), relFullRect.first.Y(), this);
	}

	base_type::SetIsVisible(value);

	if (!value && owner)
	{
		owner->GrowHor(-Width (relFullRect), relFullRect.second.X(), this);
		owner->GrowVer(-Height(relFullRect), relFullRect.second.Y(), this);
	}
}

void MovableObject::SetDisconnected()
{
	auto dv = GetDataView().lock(); 
	if (dv && IsActive())
		dv->Activate(GetOwner().lock().get());
	base_type::SetDisconnected();
}

CrdRect MovableObject::GetBorderLogicalExtents() const
{
	return (HasBorder())
		?	CrdRect(Point<CrdType>(-BORDERSIZE, -BORDERSIZE), Point<CrdType>(BORDERSIZE, BORDERSIZE))
		:	CrdRect(Point<CrdType>(0, 0), Point<CrdType>(0, 0));
}

CrdPoint MovableObject::GetBorderLogicalSize() const
{
	return (HasBorder())
		? Point<CrdType>(2*BORDERSIZE, 2*BORDERSIZE)
		: Point<CrdType>(0, 0);
}

void MovableObject::MoveTo(CrdPoint newClientRelPos) // SetClientRelPos
{
	if (newClientRelPos == m_RelPos)
		return;

	if (IsDrawn())
	{
		if (IsChildCovered() || !IsTransparent())
		{
			auto deviceDelta = ScaleCrdPoint(newClientRelPos - m_RelPos, GetScaleFactors());
			auto intDelta = CrdPoint2GPoint(deviceDelta);

			newClientRelPos = m_RelPos + ReverseGPoint(intDelta, GetScaleFactors()); // adjust move-to

			auto clipRect = GetParentClipAbsRect();
			auto scrollRect = clipRect;
			scrollRect &= GetCurrFullAbsDeviceRect();
			if (!scrollRect.empty())
			{
				auto dv = GetDataView().lock(); if (!dv) return;
				dv->ScrollDevice(intDelta, CrdRect2GRect(scrollRect), CrdRect2GRect(clipRect), this);
				TranslateDrawnRect(clipRect, intDelta);
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

CrdRect MovableObject::GetDrawnClientAbsDeviceRect() const
{
	assert(IsDrawn());
	return m_DrawnFullAbsRect & GetCurrClientAbsDeviceRect();
}

CrdRect MovableObject::GetDrawnNettAbsDeviceRect() const
{
	assert(IsDrawn());
	return m_DrawnFullAbsRect & ScaleCrdRect(GetCurrNettAbsLogicalRect(), GetScaleFactors());
}

CrdRect MovableObject::GetParentClipAbsRect() const
{
	auto owner = GetOwner().lock();
	if (owner)
		return owner->GetDrawnNettAbsDeviceRect();
	auto dv = GetDataView().lock(); if (!dv) return CrdRect();
	return GRect2CrdRect(dv->ViewDeviceRect());
}


void MovableObject::SetClientSize(CrdPoint newSize)
{
	assert(newSize.X() >= 0);
	assert(newSize.Y() >= 0);

	if (m_ClientLogicalSize == newSize)
		return;

	GrowHor(newSize.X()  - m_ClientLogicalSize.X(), m_ClientLogicalSize.X());
	GrowVer(newSize.Y()  - m_ClientLogicalSize.Y(), m_ClientLogicalSize.Y());

	assert(abs(m_ClientLogicalSize.X() - newSize.X()) < 0.1);
	assert(abs(m_ClientLogicalSize.Y() - newSize.Y()) < 0.1);
	m_ClientLogicalSize = newSize;
}

void MovableObject::SetClientRect(CrdRect r)
{
	SetClientSize(LowerBound(GetCurrClientSize(), Size(r)));
	MoveTo(TopLeft(r));
	SetClientSize(Size(r));
}

void MovableObject::SetFullRelRect(CrdRect r)
{
	if (HasBorder())
		r = Deflate(r, CrdPoint(BORDERSIZE, BORDERSIZE));
	if (!r.empty())
		SetClientRect(r);
}

void MovableObject::InvalidateClientRect(CrdRect rect) const
{
	if (!IsDrawn())
		return;

	auto crdRect = ScaleCrdRect(rect + GetCurrClientAbsLogicalPos(), GetScaleFactors());
	crdRect &= GetDrawnNettAbsDeviceRect();
	if (crdRect.empty())
		return;

	auto dv = GetDataView().lock(); if (!dv) return;
	dv->InvalidateDeviceRect( CrdRect2GRect( crdRect ) );
}

CrdPoint MovableObject::CalcClientSize() const
{
	UpdateView();
	return m_ClientLogicalSize;
}

CrdPoint MovableObject::CalcMaxSize() const
{
	return CalcClientSize() + Size(GetBorderLogicalExtents());
}

CrdPoint MovableObject::GetCurrClientAbsLogicalPos() const
{
	auto result = m_RelPos;
	auto owner = GetOwner().lock();
	while (owner)
	{
		result += owner->m_RelPos;
		owner = owner->GetOwner().lock();
	}
	return result;
}

CrdPoint MovableObject::GetCurrClientAbsLogicalPos(const GraphVisitor& v) const
{
	return m_RelPos + v.GetClientLogicalAbsPos();
}

CrdPoint MovableObject::GetCurrClientAbsDevicePos(const GraphVisitor& v) const
{ 
	return ScaleCrdPoint(GetCurrClientAbsLogicalPos(v), v.GetSubPixelFactors());
}

CrdRect  MovableObject::GetCurrClientAbsLogicalRect(const GraphVisitor& v) const 
{
	auto pos = GetCurrClientAbsLogicalPos(v); return CrdRect(pos, pos + Convert<CrdPoint>(m_ClientLogicalSize));
}

CrdRect  MovableObject::GetCurrClientAbsDeviceRect(const GraphVisitor& v) const 
{
	return ScaleCrdRect(GetCurrClientAbsLogicalRect(v), v.GetSubPixelFactors());
}

CrdRect MovableObject::GetCurrNettAbsLogicalRect(const GraphVisitor& v) const
{
	return GetCurrNettRelLogicalRect() + v.GetClientLogicalAbsPos();
}

CrdRect MovableObject::GetCurrFullAbsDeviceRect(const GraphVisitor& v) const 
{ 
	return ScaleCrdRect(GetCurrFullAbsLogicalRect(v), v.GetSubPixelFactors());
}

CrdRect MovableObject::GetCurrNettAbsLogicalRect() const
{
	return GetCurrNettRelLogicalRect() + GetCurrClientAbsLogicalPos() - m_RelPos;
}

GraphVisitState MovableObject::InviteGraphVistor(AbstrVisitor& v)
{
	return v.DoMovable(this);
}

HBITMAP MovableObject::GetAsDDBitmap(DataView* dv, CrdType subPixelFactor, MovableObject* extraObj)
{
	auto devSize = ScaleCrdPoint(CalcClientSize(), GetScaleFactors());
	auto intSize = CrdPoint2GPoint(devSize);
	SharedStrContextHandle context(mySSPrintF("while Copying %d x % u pixels to a Device Dependent Bitmap (DDB)", intSize.x, intSize.y));

	if (intSize.x >= 0x8000 || intSize.y >= 0x8000)
		throwErrorF("GDI Error", "FullExtents %s of this Graphic are above the GeoDms limit of 0x8000 rows and 0x8000 colums; consider tiling", 
			AsString(intSize).c_str()
		);

	GdiHandle<HBITMAP> hBitmap(
		CreateCompatibleBitmap(
			DcHandleBase(dv ? dv->GetHWnd() : NULL), // memDC,
			intSize.x, intSize.y
		)
	);

	CompatibleDcHandle memDC(NULL, dv ? dv->GetDefaultFont(FontSizeCategory::MEDIUM, subPixelFactor) : NULL);

	GdiObjectSelector<HBITMAP> selectBitmap(memDC, hBitmap);

	Region rgn(intSize);

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
	auto currAbsRect = GetCurrClientAbsLogicalRect();
	TType logicalX = absX / GetScaleFactors().first;
	auto margin = 4;
	if (logicalX >= currAbsRect.second.X() - margin)
		return RG_RIGHT;
	else if (logicalX <= currAbsRect.first.X() + margin)
		return RG_LEFT;
	return RG_MIDDLE;

}

void CheckDrawnRect(const MovableObject* self)
{
#if defined(MG_DEBUG)
	if (self->IsDrawn())
	{
		auto absLogicalRect = self->GetCurrFullAbsLogicalRect();
		auto sf = self->GetScaleFactors();
		auto absDeviceRect = ScaleCrdRect(absLogicalRect, sf); //absDeviceRect..Expand(1);
		auto absDeviceDrawnRect = self->GetDrawnFullAbsDeviceRect();
		auto drawn_rect_is_included_in_device_rect = IsIncluding(Inflate(absDeviceRect, CrdPoint(1,1)), absDeviceDrawnRect);
		assert(drawn_rect_is_included_in_device_rect);
	}
#endif
}

void MovableObject::GrowHor(CrdType deltaX, CrdType relPosX, const MovableObject* sourceItem)
{
	auto dv = GetDataView().lock();
	auto owner = GetOwner().lock();

	if(sourceItem)
		return;

	assert(relPosX <= m_ClientLogicalSize.X());

	if (!deltaX)
		return;

	CheckDrawnRect(this);

	auto oldFullRelRight  = Right(GetCurrFullRelLogicalRect());
	auto oldClientSize    = GetCurrClientSize();

	assert(relPosX >= 0 );
	assert(relPosX + GetCurrClientRelPos().X() <= oldFullRelRight);

	CheckDrawnRect(this);

	if (deltaX > 0)
	{
		if (owner && IsVisible()) 
			owner->GrowHor(deltaX, oldFullRelRight, this);
		m_ClientLogicalSize.X() += deltaX;
	}

	if (dv && dv->GetHWnd() && (!owner || owner->IsDrawn()) && IsVisible())
	{
		auto sf = GetScaleFactors();

		auto oldAbsRect = CrdRect(shp2dms_order<CrdType>(relPosX, 0), oldClientSize) + GetCurrClientAbsLogicalPos();
		auto clipRect   = GetParentClipAbsRect();
		if (IsDrawn())
		{
			if (IsChildCovered() || !IsTransparent())
			{
				auto be = GetBorderLogicalExtents(); be.first.X() = 0;
				oldAbsRect += Convert<CrdRect>(be);
				assert(Left(oldAbsRect) <= Right(oldAbsRect));
				assert(IsIncluding(clipRect, GetDrawnFullAbsDeviceRect())); // invariant IsIncluding relation

				auto oldAbsGRect = ScaleCrdRect(oldAbsRect, sf);
				auto oldVisibleAbsRect = oldAbsGRect & GetDrawnFullAbsDeviceRect();

				assert(IsIncluding(clipRect, oldVisibleAbsRect)); // follows from previous assertion and clipping

				auto intDeltaX = CrdType2GType( ((relPosX + deltaX) * sf.first) - (relPosX * sf.first) );
				auto devDelta = GPoint(intDeltaX, 0);
				if (!oldVisibleAbsRect.empty())
					dv->ScrollDevice(devDelta, CrdRect2GRect(oldVisibleAbsRect), CrdRect2GRect(clipRect), this);
				ResizeDrawnRect(clipRect, devDelta, GPoint(oldAbsGRect.first.X(), MaxValue<GType>()));

				assert(!IsDrawn() || IsIncluding(clipRect, GetDrawnFullAbsDeviceRect())); // invariant IsIncluding relation

				// DEBUG
				if (deltaX < 0)
					oldAbsRect.first.X() = oldAbsRect.second.X() + deltaX;
				else
					oldAbsRect.second.X() = oldAbsRect.first.X() + deltaX;
				oldAbsRect &= ReverseCrdRect(clipRect, sf);
				if (!oldAbsRect.empty())
				{
					oldAbsGRect = ScaleCrdRect(oldAbsRect, sf);
					dv->InvalidateDeviceRect(CrdRect2GRect(oldAbsGRect));
				}
			}
			else
				InvalidateDraw();
		}
		if (!IsDrawn() && deltaX > 0)
		{
			oldAbsRect.first .X() = oldAbsRect.second.X();
			oldAbsRect.second.X() = oldAbsRect.first .X() + deltaX;
			clipRect &= ScaleCrdRect(oldAbsRect, sf);
			if (!clipRect.empty())
				dv->InvalidateDeviceRect(CrdRect2GRect(clipRect));
		}
	}

	if (deltaX < 0)
		m_ClientLogicalSize.X() += deltaX;

	CheckDrawnRect(this);

	if (owner && IsVisible()) 
	{
		if (deltaX < 0)
			owner->GrowHor(deltaX, oldFullRelRight, this);
	}

	CheckDrawnRect(this);
}

void MovableObject::GrowVer(CrdType deltaY, CrdType relPosY, const MovableObject* sourceItem)
{
	auto dv = GetDataView().lock();
	auto owner = GetOwner().lock();

	if(sourceItem)
		return;

	assert(relPosY <= m_ClientLogicalSize.Y());

	if (!deltaY)
		return;

	CheckDrawnRect(this);

	auto oldFullRelBottom = Bottom(GetCurrFullRelLogicalRect());
	auto oldClientSize    = GetCurrClientSize();

	assert(relPosY >= 0 );
	assert(relPosY + GetCurrClientRelPos().Y() <= oldFullRelBottom);

	CheckDrawnRect(this);

	if (deltaY > 0)
	{
		if (owner && IsVisible()) 
			owner->GrowVer(deltaY, oldFullRelBottom, this);
		m_ClientLogicalSize.Y() += deltaY;
	}


	if (dv && dv->GetHWnd() && ( !owner || owner->IsDrawn()) && IsVisible())
	{
		auto sf = GetScaleFactors();

		auto oldAbsRect = CrdRect(shp2dms_order<CrdType>(0, relPosY), oldClientSize) + GetCurrClientAbsLogicalPos();
		auto clipRect   = GetParentClipAbsRect();
		if (IsDrawn())
		{
			if (IsChildCovered() || !IsTransparent())
			{
				auto be = GetBorderLogicalExtents(); be.first.Y() = 0;
				oldAbsRect += Convert<CrdRect>(be);
				assert(Top(oldAbsRect) <= Bottom(oldAbsRect));
				assert(IsIncluding(clipRect, GetDrawnFullAbsDeviceRect())); // invariant IsIncluding relation

				auto oldAbsGRect = ScaleCrdRect(oldAbsRect, sf);
				auto oldVisibleAbsRect = oldAbsGRect & GetDrawnFullAbsDeviceRect();

				assert(IsIncluding(clipRect, oldVisibleAbsRect)); // follows from previous assertion and clipping

				auto intDeltaY = CrdType2GType(((relPosY + deltaY) * sf.first) - (relPosY * sf.first));
				auto devDelta = GPoint(0, intDeltaY);
				if (!oldVisibleAbsRect.empty())
					dv->ScrollDevice(devDelta, CrdRect2GRect(oldVisibleAbsRect), CrdRect2GRect(clipRect), this);
				ResizeDrawnRect(clipRect, devDelta, GPoint(MaxValue<GType>(), oldAbsGRect.first.Y()));

				assert(!IsDrawn() || IsIncluding(clipRect, GetDrawnFullAbsDeviceRect())); // invariant IsIncluding relation

				// DEBUG
				if (deltaY < 0)
					oldAbsRect.first.Y()    = oldAbsRect.second.Y() + deltaY;
				else
					oldAbsRect.second.Y() = oldAbsRect.first.Y()    + deltaY;
				oldAbsRect &= ReverseCrdRect(clipRect, sf);
				if (!oldAbsRect.empty())
				{
					oldAbsGRect = ScaleCrdRect(oldAbsRect, sf);
					dv->InvalidateDeviceRect(CrdRect2GRect(oldAbsGRect));
				}
			}
			else
				InvalidateDraw();
		}
		if (!IsDrawn() && deltaY > 0)
		{
			oldAbsRect.first.Y()   = oldAbsRect.second.Y();
			oldAbsRect.second.Y() = oldAbsRect.first.Y()     + deltaY;
			clipRect &= ScaleCrdRect(oldAbsRect, sf);
			if (!clipRect.empty())
				dv->InvalidateDeviceRect(CrdRect2GRect(clipRect));
		}

	}

	if (deltaY < 0)
		m_ClientLogicalSize.Y() += deltaY;

	CheckDrawnRect(this);

	if (owner && IsVisible())
	{
		if (deltaY < 0) 
			owner->GrowVer(deltaY, oldFullRelBottom, this);
	}

	CheckDrawnRect(this);
}

#if defined(MG_DEBUG)
void MovableObject::CheckState() const
{
	base_type::CheckState();
	CheckDrawnRect(this);
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

