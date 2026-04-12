// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

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
#include "WmsLayer.h"

GraphicRect::GraphicRect(ScalableObject* owner)
	:	ScalableObject(owner)
	,	m_PenColor  ( CombineRGB(0x0,  0x00, 0xFF) )
	,	m_BlendColor( CombineRGB(0xFF, 0x00, 0x80) )
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
		auto wmsLayer = targetVP->FindBackgroundWmsLayer();
		if (wmsLayer) wmsLayer->ZoomOut(targetVP.get(), true);
		UpdateExtents();
	}
}

TRect GraphicRect::GetBorderLogicalExtents() const
{
	return TRect(-1, -1, 1, 1);  // max rounding error without considering orientation
}

bool GraphicRect::MouseEvent(MouseEventDispatcher& med)
{
	if (med.GetEventInfo().m_EventID & EventID::LBUTTONDOWN )
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
	if (Width(clientRect) <= 0 || Height(clientRect) <= 0)
		return false;

	auto* drawCtx = d.GetDrawContext();

	{
		OrientationType orientation = d.GetTransformation().Orientation();

		CrdType topRow = MustNegateY(orientation) ? wr.second.Row() : wr.first .Row();
		CrdType botRow = MustNegateY(orientation) ? wr.first .Row() : wr.second.Row();
		CrdType lefCol = MustNegateX(orientation) ? wr.second.Col() : wr.first .Col();
		CrdType rigCol = MustNegateX(orientation) ? wr.first .Col() : wr.second.Col();

		// top line
		if (topRow >= cr.first.Row() && topRow <= cr.second.Row())
			drawCtx->DrawLine(GPoint(clientRect.left, clientRect.top), GPoint(clientRect.right, clientRect.top), m_PenColor);
		if (++clientRect.top == clientRect.bottom)
			return false;

		// left line
		if (lefCol >= cr.first.Col() && lefCol <= cr.second.Col())
			drawCtx->DrawLine(GPoint(clientRect.left, clientRect.top), GPoint(clientRect.left, clientRect.bottom), m_PenColor);
		if (++clientRect.left == clientRect.right)
			return false;

		// bottom line
		if (botRow >= cr.first.Row() && botRow <= cr.second.Row())
		{
			--clientRect.bottom;
			drawCtx->DrawLine(GPoint(clientRect.left, clientRect.bottom), GPoint(clientRect.right, clientRect.bottom), m_PenColor);
		}
		if (clientRect.top == clientRect.bottom)
			return false;

		// right line
		if (rigCol >= cr.first.Col() && rigCol <= cr.second.Col())
		{
			--clientRect.right;
			drawCtx->DrawLine(GPoint(clientRect.right, clientRect.top), GPoint(clientRect.right, clientRect.bottom), m_PenColor);
		}
		if (clientRect.left == clientRect.right)
			return false;
	}
	if ( (Width(clientRect) > 2 && Height(clientRect) > 2) || !IsIncluding(cr, wr) )
	{
		DmsColor blendColor = CombineRGB(GetRed(m_BlendColor), GetGreen(m_BlendColor), GetBlue(m_BlendColor), 0xDF);
		drawCtx->FillRect(clientRect, blendColor);
	}
	return true;
}

bool GraphicRect::Draw(GraphDrawer& d) const
{
	if (!d.DoDrawData())
		return false;

	auto wr= CalcWorldClientRect();
	CrdRect cr = d.GetWorldClipRect();

	auto clientRect = GetClippedCurrFullAbsDeviceRect(d);

	if (!DrawRect(d, wr, cr, CrdRect2GRect(clientRect)))
		return false;
	auto deviceROI = DRect2GRect(m_ROI, d.GetTransformation());
	deviceROI += TRect2GRect(TRect(-1, -1, 1, 1), d.GetSubPixelFactors());

	DrawRect(d, m_ROI, cr, deviceROI); 

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
