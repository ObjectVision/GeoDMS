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
#pragma hdrstop

#include "ShvUtils.h"
#include "ScalableObject.h"

#include "act/MainThread.h"
#include "dbg/DebugCast.h"
#include "geo/Conversions.h"
#include "utl/mySPrintF.h"
#include "mci/Class.h"
#include "utl/IncrementalLock.h"
#include "utl/mySPrintF.h"

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

GRect ScalableObject::GetCurrFullAbsDeviceRect(const GraphVisitor& v) const
{
	auto cwcr = GetCurrWorldClientRect();
	if (cwcr.empty())
		return GRect();

	return DRect2GRect(cwcr, v.GetTransformation()) 
		+ TRect2GRect(GetBorderLogicalExtents(), v.GetSubPixelFactors());
}

CrdRect ScalableObject::CalcWorldClientRect() const
{
	dms_assert(IsMainThread());
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

#include "ViewPort.h"

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

void ScalableObject::InvalidateWorldRect(const CrdRect& rect, const TRect* borderExtentsPtr) const 
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
	if (AllVisible())
	{
		if (HasNoExtent())
			GetViewPort()->InvalidateDraw();
		else
			InvalidateWorldRect(CalcWorldClientRect(), nullptr);
	}
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


		menuData.push_back( MenuItem(SharedStr("to &Top"   ), new LayerShuffleCmd(this, ElemShuffleCmd::ToFront ), ls.get(), isFront     ? MFS_GRAYED : 0 ) );
		menuData.push_back( MenuItem(SharedStr("to &Bottom"), new LayerShuffleCmd(this, ElemShuffleCmd::ToBack  ), ls.get(), isBack      ? MFS_GRAYED : 0 ) );
		menuData.push_back( MenuItem(SharedStr("&Up"       ), new LayerShuffleCmd(this, ElemShuffleCmd::Forward ), ls.get(), isNearFront ? MFS_GRAYED : 0 ) );
		menuData.push_back( MenuItem(SharedStr("&Down"     ), new LayerShuffleCmd(this, ElemShuffleCmd::Backward), ls.get(), isNearBack  ? MFS_GRAYED : 0 ) );
	}

//	Remove Layer
	menuData.push_back( MenuItem(SharedStr("&Remove Layer"), new LayerShuffleCmd(this, ElemShuffleCmd::Remove), ls.get() ));
}

IMPL_ABSTR_CLASS(ScalableObject)
