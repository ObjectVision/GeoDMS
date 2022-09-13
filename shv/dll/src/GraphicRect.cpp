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

#include "GraphicRect.h"

#include "act/ActorVisitor.h"
#include "dbg/DebugContext.h"
#include "geo/Transform.h"
#include "geo/Conversions.h"

#include "Param.h"
#include "TreeItemClass.h"

#include "ShvUtils.h"

#include "AbstrController.h"
#include "GraphVisitor.h"
#include "Controllers.h"
#include "CounterStacks.h"
#include "DataView.h"
#include "MapControl.h"
#include "MouseEventDispatcher.h"
#include "ViewPort.h"

HBITMAP CreateBlender()
{
	BITMAPINFOHEADER bmiHeader;
	bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
	bmiHeader.biWidth         = 1;
	bmiHeader.biHeight        = 1;
	bmiHeader.biPlanes        = 1;
	bmiHeader.biBitCount      = 24;
	bmiHeader.biCompression   = BI_RGB;
	bmiHeader.biSizeImage     = 0;
	bmiHeader.biXPelsPerMeter = 0;
	bmiHeader.biYPelsPerMeter = 0;
	bmiHeader.biClrUsed       = 0;
	bmiHeader.biClrImportant  = 0;

	BYTE* pvBits = 0;
	
	HBITMAP result = CreateDIBSection(
		0, // m_hDC,
		reinterpret_cast<BITMAPINFO*>(&bmiHeader),
		DIB_RGB_COLORS,
		&reinterpret_cast<void*&>(pvBits),
		NULL, 0
	);
	CheckedGdiCall(result, "CreateDIBSection");
	dms_assert(pvBits);

	*pvBits++ = 0x80; // BLUE
	*pvBits++ = 0x00; // GREEN
	*pvBits++ = 0xFF; // RED
	return result;
}

GraphicRect::GraphicRect(ScalableObject* owner)
	:	ScalableObject(owner)
	,	m_Pen  ( CreatePen       (PS_SOLID, 0,  RGB(0x0,  0x00, 0xFF)), "GraphicRect::CreatePen" )
#if defined(SHV_ALPHABLEND_NOT_SUPPORTED)
	,	m_Brush( CreateHatchBrush(HS_DIAGCROSS, RGB(0xFF, 0x00, 0x80)), "GraphicRect::CreateSolidBrush" )
#else
	,	m_BlendBitmap(CreateBlender())
#endif
{}

GraphicRect::~GraphicRect()
{
	auto tvp = GetTargetVP().lock();
	if (tvp) tvp->m_Tracker = nullptr;
}

std::weak_ptr<MapControl> GraphicRect::GetMapControl()
{
	auto owner = GetOwner().lock();
	while (owner) {
		dms_assert(owner);
		std::shared_ptr<MapControl> ownerAsMapControl = std::dynamic_pointer_cast<MapControl>(owner);
		if (ownerAsMapControl)
			return ownerAsMapControl;
		owner = owner->GetOwner().lock();
	};
	return std::weak_ptr<MapControl>();
}

std::weak_ptr<ViewPort> GraphicRect::GetTargetVP()
{
	auto tvp = m_TargetVP.lock();
	if (!tvp)
	{
		auto mc = GetMapControl().lock();
		if (mc && mc->GetOverviewPort())
			m_TargetVP = mc->GetOverviewPort()->shared_from_base<ViewPort>();
	}
	return m_TargetVP;
}

std::weak_ptr<ViewPort> GraphicRect::GetSourceVP()
{
	auto svp = m_SourceVP.lock();
	if (!svp)
	{
		auto mc = GetMapControl().lock();
		if (mc && mc->GetViewPort())
			m_SourceVP = mc->GetViewPort()->shared_from_base<ViewPort>();
	}
	return m_SourceVP;
}


void GraphicRect::SetROI(const CrdRect& roi)
{
	if (m_ROI == roi)
		return;
	m_ROI = roi;
}

void GraphicRect::UpdateExtents()
{
	auto sourceVP = GetSourceVP().lock();
	if (!sourceVP)
		return;

	sourceVP->UpdateView();
	SetWorldClientRect(sourceVP->CalcWorldClientRect());
	SetROI(sourceVP->GetROI());
}

void GraphicRect::DoUpdateView()
{
	UpdateExtents();

	auto targetVP = GetTargetVP().lock();
	targetVP->m_Tracker = this;
	targetVP->InvalidateView();
}

const Float64 NearbyFactor = 1.00001;

bool IsNearlyEqual(Float64 a, Float64 b, Float64 factor = NearbyFactor)
{
	return IsTouching(Range<Float64>(a, a*factor), Range<Float64>(b, b*factor));
}

template<typename T>
bool IsNearlyEqual(const Point<T>& a, const Point<T>& b, Float64 factor = NearbyFactor)
{
	return IsNearlyEqual(a.first , b.first , factor)
		&& IsNearlyEqual(a.second, b.second, factor);
}

template<typename T>
bool IsNearlyEqual(const Range<T>& a, const Range<T>& b, Float64 factor = NearbyFactor)
{
	return IsNearlyEqual(Size(a | b), Size(a & b), factor);
}

void GraphicRect::AdjustTargetVieport()
{
	UpdateExtents();

	auto targetVP = GetTargetVP().lock();
	if (GetCurrWorldClientRect().empty() || !targetVP)
		return;

	CrdRect  wr  = GetCurrWorldClientRect();            // == sourceVP->WorldRect
	CrdPoint wrs = Size(wr);
	CrdPoint wrc = Center(wr);
	CrdRect  wr2 = Inflate(wrc, wrs*CrdType(2)); // == 2*wr

	CrdRect tr  = targetVP->GetROI();
	if (IsIncluding(tr, wr2)) {
		CrdPoint center = Center(tr);
		MakeUpperBound(center, wr2.first);
		MakeLowerBound(center, wr2.second);
		wrc = center;
	}
	CrdRect trr = Inflate(wrc, wrs*CrdType(4));
	if (!IsNearlyEqual(tr, trr))
	{
		targetVP->SetROI(trr);
		UpdateExtents();
	}
}

GRect GraphicRect::GetBorderPixelExtents(CrdType subPixelFactor) const
{
	return GRect(-1, -1, 1, 1);  // max rounding error without considering orientation
}

bool GraphicRect::MouseEvent(MouseEventDispatcher& med)
{
	if (med.GetEventInfo().m_EventID & EID_LBUTTONDOWN )
	{
		auto owner = med.GetOwner().lock();
		owner->InsertController(
			new RectPanController(
				owner.get(), 
				this,
				med.GetTransformation(), 
				med.GetEventInfo().m_Point
			)
		);
		return true;
	}
	return false;
}

void GraphicRect::MoveWorldRect(const CrdPoint& point)
{
	UpdateView(); // get m_ROI and CurrClientW

	CrdRect rect = m_ROI + point;

	SetWorldClientRect( GetCurrWorldClientRect() + point );

	auto svp = GetSourceVP().lock();
	if (svp)
		svp->SetROI(rect);
//	ChangePoint(const_cast<AbstrDataItem*>(m_Source_TL.get_ptr()), rect.first , false);
//	ChangePoint(const_cast<AbstrDataItem*>(m_Source_BR.get_ptr()), rect.second, false);
	InvalidateView(); // 
//	GetTargetVP()->InvalidateView();
//	AdjustTargetVieport();
}


bool GraphicRect::DrawRect(GraphDrawer& d, const CrdRect& wr, const CrdRect& cr, GRect clientRect) const
{
	if (_Width(clientRect) <= 0 || _Height(clientRect) <= 0)
		return false;

	HDC dc = d.GetDC();

	{
		GdiObjectSelector<HPEN> penHolder(dc, m_Pen);

		OrientationType orientation = d.GetTransformation().Orientation();

		CrdType topRow = MustNegateY(orientation) ? wr.second.Row() : wr.first .Row();
		CrdType botRow = MustNegateY(orientation) ? wr.first .Row() : wr.second.Row();
		CrdType lefCol = MustNegateX(orientation) ? wr.second.Col() : wr.first .Col();
		CrdType rigCol = MustNegateX(orientation) ? wr.first .Col() : wr.second.Col();

		// top line
		if (topRow >= cr.first.Row() && topRow <= cr.second.Row())
		{
			MoveToEx(dc, clientRect.left,  clientRect.top, NULL);
			LineTo  (dc, clientRect.right, clientRect.top);
		}
		if (++clientRect.top == clientRect.bottom)
			return false;

		// left line
		if (lefCol >= cr.first.Col() && lefCol <= cr.second.Col())
		{
			MoveToEx(dc, clientRect.left, clientRect.top, NULL);
			LineTo  (dc, clientRect.left, clientRect.bottom);
		}
		if (++clientRect.left == clientRect.right)
			return false;

		// bottom line
		if (botRow >= cr.first.Row() && botRow <= cr.second.Row())
		{
			--clientRect.bottom;
			MoveToEx(dc, clientRect.left,  clientRect.bottom, NULL);
			LineTo  (dc, clientRect.right, clientRect.bottom);
		}
		if (clientRect.top == clientRect.bottom)
			return false;

		// right line
		if (rigCol >= cr.first.Col() && rigCol <= cr.second.Col())
		{
			--clientRect.right;
			MoveToEx(dc, clientRect.right, clientRect.top, NULL);
			LineTo  (dc, clientRect.right, clientRect.bottom);
		}
		if (clientRect.left == clientRect.right)
			return false;
	}
	if ( (_Width(clientRect) > 2 && _Height(clientRect) > 2) || !IsIncluding(cr, wr) )
	{

#if defined(SHV_ALPHABLEND_NOT_SUPPORTED)

		DcBkModeSelector xxx(dc, TRANSPARENT);
		FillRect(dc, &sr, m_Brush);

#else

		CompatibleDcHandle memDC(NULL, 0);
		GdiObjectSelector<HBITMAP> selectBitmap(memDC, m_BlendBitmap);

		BLENDFUNCTION blendFunction;
			blendFunction.BlendOp             = AC_SRC_OVER;
			blendFunction.BlendFlags          = 0;
			blendFunction.SourceConstantAlpha = 0x20;
			blendFunction.AlphaFormat         = 0; // not AC_SRC_ALPHA


		CheckedGdiCall(
			AlphaBlend(
				dc, 
					clientRect.left, 
					clientRect.top, 
					_Width(clientRect), 
					_Height(clientRect),
				memDC, 0,0, 1,1,
				blendFunction
			),
			"AlphaBlend"
		);

#endif
	}
	return true;
}

bool GraphicRect::Draw(GraphDrawer& d) const
{
	if (!d.DoDrawData())
		return false;

	auto wr= CalcWorldClientRect();
	CrdRect cr = d.GetWorldClipRect();

//	TRect sr = Convert<TRect>( d.GetTransformation().Apply(wr & cr) );
	GRect clientRect = GetClippedCurrFullAbsRect(d);

	if (!DrawRect(d, wr, cr, clientRect))
		return false;
	DrawRect(d, m_ROI, cr, 
			TRect2GRect( Convert<TRect>(d.GetTransformation().Apply(m_ROI)) + TRect(-1,-1, 1, 1) )
	); 

	return false;
}


//	override virtuals of Actor
ActorVisitState GraphicRect::VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const
{
	auto svp = m_SourceVP.lock();
	if (svp) if (svp->VisitSuppliers(svf, visitor) == AVS_SuspendedOrFailed) return AVS_SuspendedOrFailed;
	return base_type::VisitSuppliers(svf, visitor);
}

#include "LayerClass.h"

IMPL_DYNC_SHVCLASS(GraphicRect,ScalableObject)
