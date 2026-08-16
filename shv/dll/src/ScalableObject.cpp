// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "ShvDllPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// ScalableObject: base of the world-coordinate graphic objects — extents,
// transformations and viewport access.

#include "ShvUtils.h"
#include "ScalableObject.h"

#include "act/MainThread.h"
#include "dbg/DebugCast.h"
#include "vt/Conversions.h"
#include "geom/PointOrder.h"
#include "geom/Range.h"
#include "utl/StrFormat.h"
#include "mci/Class.h"
#include "utl/IncrementalLock.h"

#include "AbstrCmd.h"
#include "DataView.h"
#include "GraphicObject.h"
#include "LayerSet.h"
#include "MenuData.h"
#include "ViewPort.h"

//----------------------------------------------------------------------
// struct : LayerShuffleCmd
//----------------------------------------------------------------------

struct LayerShuffleCmd : public AbstrCmd
{
	LayerShuffleCmd(ScalableObject* layerObj, ElemShuffleCmd esc)
		:	m_LayerObj(layerObj->shared_from_base<ScalableObject>())
		,	m_ElemShuffleCmd(esc)
	{}

	GraphVisitState DoLayerSet(LayerSet* ls) override
	{
		ls->ShuffleEntry(m_LayerObj.get(), m_ElemShuffleCmd);
		return GVS_Handled;
	}

	std::shared_ptr<ScalableObject> m_LayerObj;
	ElemShuffleCmd                  m_ElemShuffleCmd;
};

//----------------------------------------------------------------------
// class: ScalableObject --- size and positioning
//----------------------------------------------------------------------

ScalableObject::ScalableObject(GraphicObject* owner)
	:	base_type(owner)
{
	m_State.Set(GOF_Transparent);
}

//----------------------------------------------------------------------
// class: ScalableObject --- size and positioning
//----------------------------------------------------------------------

CrdRect ScalableObject::GetCurrFullAbsDeviceRect(const GraphVisitor& v) const
{
	auto cwcr = GetCurrWorldClientRect();
	if (cwcr.empty())
		return CrdRect();

	auto tr = v.GetTransformation();
	auto cwDeviceRect = tr.Apply(cwcr);

	// Under a projective (tilted) view the layer's full world rect can straddle the projective horizon
	// (the locus w == 0). A corner beyond the horizon maps through a non-positive denominator and yields
	// garbage device coords, so the axis-aligned bounding box from Apply() is unreliable and the
	// IsIntersecting cull in GraphVisitor::Visit would wrongly drop the WHOLE layer (e.g. a nation-wide BAG
	// layer vanishes on the first tilt step). When the rect crosses the horizon, fall back to the current
	// clip device rect so the layer is still visited; the finer per-feature culls inside the draw (which use
	// the bounded visible rect) then limit the actual work. ApplyDenom() returns 1 for <= Affine2D, so yaw/
	// affine and the level-c separable path are byte-identical (no corner ever "crosses").
	if (!tr.IsAxisSeparable())
	{
		auto     devClip = g2dms_order<CrdType>(v.GetAbsClipDeviceRect());
		CrdType  refW    = tr.ApplyDenom(tr.Reverse(Center(devClip)));
		CrdType  refSign = (refW >= 0) ? 1.0 : -1.0;
		const CrdPoint corners[4] = {
			cwcr.first,
			CrdPoint(cwcr.second.first, cwcr.first.second),
			CrdPoint(cwcr.first.first,  cwcr.second.second),
			cwcr.second
		};
		for (const auto& c : corners)
			if (tr.ApplyDenom(c) * refSign <= 0.0)
			{
				cwDeviceRect = GRect2CrdRect(v.GetAbsClipDeviceRect());
				break;
			}
	}

	auto borderDeviceExtents = TRect2CrdRect(GetBorderLogicalExtents(), v.GetSubPixelFactors());
	return cwDeviceRect + borderDeviceExtents;
}

CrdRect ScalableObject::CalcWorldClientRect() const
{
	assert(IsMainThread());
	UpdateView();
	return m_WorldClientRect;
}

void ScalableObject::SetWorldClientRect(const CrdRect& worldClientRect)
{
	if (worldClientRect == m_WorldClientRect)
		return;

	InvalidateDraw(); // invalidates the current m_DrawnFullAbsRect if that was drawn
	m_WorldClientRect = worldClientRect;
	InvalidateWorldRect( m_WorldClientRect, nullptr ); // invalidates the new rect that must be drawn
	auto owner = GetOwner().lock();
	if (owner)
		owner->InvalidateView();
}

CrdRect ScalableObject::GetCurrWorldClientRect() const
{
	return m_WorldClientRect;
}

TRect ScalableObject::GetBorderLogicalExtents() const
{
	return TRect(0, 0, 0, 0);
}

//----------------------------------------------------------------------
// class: ScalableObject --- accessing related-entries by supplying covariant return types
//----------------------------------------------------------------------

const ViewPort* ScalableObject::GetViewPort() const
{
	auto parent = GetOwner().lock();
	while (parent)
	{
		const ViewPort* result = dynamic_cast<const ViewPort*>(parent.get());
		if (result)
			return result;
		parent = parent->GetOwner().lock();
	}
	return nullptr;
}

void ScalableObject::InvalidateWorldRect(CrdRect rect, const TRect* borderExtentsPtr) const 
{
	const ViewPort* vp = GetViewPort();
	if (!vp)
		return;

	vp->InvalidateWorldRect(
		rect
	,	borderExtentsPtr
		?	*borderExtentsPtr
		: GetBorderLogicalExtents()
	);
}

ScalableObject* ScalableObject::GetEntry(SizeT i)
{
	return debug_cast<ScalableObject*>( base_type::GetEntry(i) );
}

//----------------------------------------------------------------------
// class: ScalableObject --- override Actor
//----------------------------------------------------------------------

void ScalableObject::DoInvalidate() const
{
	dms_assert(!m_State.HasInvalidationBlock());

	base_type::DoInvalidate();
	const_cast<ScalableObject*>(this)->InvalidateDraw();

	dms_assert(DoesHaveSupplInterest() || !GetInterestCount());
}

//----------------------------------------------------------------------
// class: ScalableObject --- override GraphicObject
//----------------------------------------------------------------------

void ScalableObject::Sync(TreeItem* viewContext, ShvSyncMode sm)
{
	dms_assert(viewContext);
	base_type::Sync(viewContext, sm);

	SyncState(this, viewContext, GetTokenID_mt("DetailsVisible"), SOF_DetailsVisible, true, sm);
	SyncState(this, viewContext, GetTokenID_mt("Topographic"), SOF_Topographic, false, sm);
}


void ScalableObject::OnVisibilityChanged()
{
	base_type::OnVisibilityChanged();
	// A layer becoming visible changes the viewport, so force a full repaint. The previous targeted
	// InvalidateWorldRect(CalcWorldClientRect()) was unreliable: ViewPort::InvalidateWorldRect projects the
	// world rect with Apply(), which under a projective (tilted) view is ApplyBounds and collapses across the
	// horizon to a garbage/inverted device rect -> nothing was invalidated, so REACTIVATING a layer (double-
	// click in the LayerControl) did NOT redraw until the next navigation action.
	if (AllVisible())
		if (auto vp = GetViewPort())
			vp->InvalidateDraw();
	m_cmdVisibilityChanged();

	if ((AllVisible() && !DetailsTooLong()) != DetailsVisible())
		ToggleDetailsVisibility();
}

//----------------------------------------------------------------------
// class: ScalableObject --- commands
//----------------------------------------------------------------------

void ScalableObject::ToggleDetailsVisibility()
{
	m_State.Toggle(SOF_DetailsVisible);
	m_cmdDetailsVisibilityChanged();

	if (GetContext())
		SaveValue<Bool>(GetContext(), GetTokenID_mt("DetailsVisible"), DetailsVisible());
}

//----------------------------------------------------------------------
// class: ScalableObject --- LayerControl support
//----------------------------------------------------------------------

std::shared_ptr<LayerControlBase> ScalableObject::CreateControl(MovableObject* owner)
{
	throwIllegalAbstract(MG_POS, "CreateControl");
}


SharedStr ScalableObject::GetCaption() const
{
	return SharedStr(GetDynamicClass()->GetID());
}

void ScalableObject::FillLcMenu(MenuData& menuData)
{
	SharedStr      captionStr = GetCaption();
	CharPtr        caption    = captionStr.c_str();

	auto ls = GetOwner().lock(); if (!ls) return;

//	subMenu Order
//	menuData.AddSeparator();
	SizeT n = ls->NrEntries();
	dms_assert(n > 0);
	if (n > 1)
	{
		SubMenu subMenu(menuData, SharedStr("Bring &z-Position of Layer"));

		dms_assert(ls);
		bool isFront     = (!n) || ls->GetEntry(n-1) == this;
		bool isNearFront = isFront || (n < 2) || (ls->GetEntry(n-2) == this);
		bool isBack      = (!n) || ls->GetEntry(0) == this;
		bool isNearBack  = isBack  || (n < 2) || (ls->GetEntry(1) == this);


		menuData.push_back( MenuItem(SharedStr("to &Top"   ), std::make_unique<LayerShuffleCmd>(this, ElemShuffleCmd::ToFront ), ls.get(), isFront     ? MFS_GRAYED : 0 ) );
		menuData.push_back( MenuItem(SharedStr("to &Bottom"), std::make_unique<LayerShuffleCmd>(this, ElemShuffleCmd::ToBack  ), ls.get(), isBack      ? MFS_GRAYED : 0 ) );
		menuData.push_back( MenuItem(SharedStr("&Up"       ), std::make_unique<LayerShuffleCmd>(this, ElemShuffleCmd::Forward ), ls.get(), isNearFront ? MFS_GRAYED : 0 ) );
		menuData.push_back( MenuItem(SharedStr("&Down"     ), std::make_unique<LayerShuffleCmd>(this, ElemShuffleCmd::Backward), ls.get(), isNearBack  ? MFS_GRAYED : 0 ) );
	}

//	Remove Layer
	menuData.push_back( MenuItem(SharedStr("&Remove Layer"), std::make_unique<LayerShuffleCmd>(this, ElemShuffleCmd::Remove), ls.get() ));
}

IMPL_ABSTR_CLASS(ScalableObject)
