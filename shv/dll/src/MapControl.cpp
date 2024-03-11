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

#include "MapControl.h"

#include "dbg/DebugContext.h"
#include "geo/Conversions.h"
#include "geo/PointOrder.h"
#include "mci/Class.h"

#include "AbstrDataItem.h"

#include "ShvUtils.h"

#include "AbstrCmd.h"
#include "DataView.h"
#include "GraphDataView.h"
#include "GraphicLayer.h"
#include "GraphVisitor.h"
#include "KeyFlags.h"
#include "LayerControl.h"
#include "LayerSet.h"
#include "MouseEventDispatcher.h"
#include "ScaleBar.h"
#include "ScrollPort.h"
#include "ViewPort.h"

//----------------------------------------------------------------------
// class  : MapControl
//----------------------------------------------------------------------

MapControl::MapControl(DataView* dv)
	: ViewControl(dv)
{}

void MapControl::Init(DataView* dv)
{
	assert(dv);
	m_ViewPort = make_shared_gr<ViewPort>(this, dv, "MapView")();
	m_LayerSet = make_shared_gr<LayerSet>(m_ViewPort.get())();

	m_ScrollPort = make_shared_gr<ScrollPort>(this, dv, "LayerControls", false)();
	m_LayerControlSet = make_shared_gr<LayerControlSet>(m_ScrollPort.get(), m_LayerSet.get())();

	m_OvViewPort = make_shared_gr<ViewPort>(this, dv, "Overview")();
	m_OvLayerSet = make_shared_gr<LayerSet>(m_OvViewPort.get())();

	InsertEntry(m_ScrollPort.get());
	InsertEntry(m_ViewPort.get());
	InsertEntry(m_OvViewPort.get());

	m_ViewPort  ->SetContents(m_LayerSet);
	m_ScrollPort->SetContents(m_LayerControlSet);
	m_OvViewPort->SetContents(m_OvLayerSet);


	dv->Activate(m_ScrollPort.get());

	m_OvViewPort->SetIsVisible(false);
};

//	locate call to destructors of LayerSet and ScrollPort here to reduce dependencies of the header file when included in other code units
MapControl::~MapControl()
{}

GraphVisitState MapControl::InviteGraphVistor(AbstrVisitor& v)
{
	return v.DoMapControl(this);
}

void MapControl::ProcessSize(CrdPoint mapControlSize)
{
	auto viewPortSize = mapControlSize;

	if (m_ScrollPort->IsVisible())
		viewPortSize.X() -= mapControlSize.X() / 4L;

	bool viewPortSmaller = (viewPortSize.X() < m_ViewPort->GetCurrClientSize().X());
	if (viewPortSmaller)
		m_ViewPort->SetClientRect( CrdRect( Point<CrdType>(0, 0), viewPortSize ) );

	TType layerControlStartRow = m_OvViewPort->IsVisible() ? Min<CrdType>(250, mapControlSize.Y() / 3) : 0;
	TType layerControlEndRow   = mapControlSize.Y();

	bool overviewSmaller = (layerControlStartRow < m_OvViewPort->GetCurrClientSize().Y());
	if (overviewSmaller)
		m_OvViewPort->SetClientRect( CrdRect( shp2dms_order<CrdType>(viewPortSize.X(), 0), shp2dms_order<CrdType>(mapControlSize.X(), layerControlStartRow)) );

	m_ScrollPort->SetClientRect(
		CrdRect(
			shp2dms_order<CrdType>(viewPortSize  .X(), layerControlStartRow),
			shp2dms_order<CrdType>(mapControlSize.X(), layerControlEndRow)
		)
	);
	if (!overviewSmaller)
		m_OvViewPort->SetClientRect( CrdRect(shp2dms_order<CrdType>(viewPortSize.X(), 0), shp2dms_order<CrdType>(mapControlSize.X(), layerControlStartRow)) );
	if (!viewPortSmaller)
		m_ViewPort->SetClientRect( CrdRect( Point<CrdType>(0, 0), viewPortSize ) );
}

void MapControl::OnChildSizeChanged()
{
}

void MapControl::ToggleOverview()
{
	GetOverviewPort()->ToggleVisibility();

	ProcessSize(GetCurrClientSize());
}

void MapControl::ToggleLayerControl()
{
	GetScrollPort()->ToggleVisibility();

	ProcessSize(GetCurrClientSize());
}

bool MapControl::OnKeyDown(UInt32 virtKey)
{
	if (KeyInfo::IsCtrl(virtKey))
	{
		switch (KeyInfo::CharOf(virtKey)) {
			case 'G': return OnCommand(TB_CopyLC); 
		}
	}
	return base_type::OnKeyDown(virtKey)  || GetViewPort()->OnKeyDown(virtKey);
}

void MapControl::SetLayout(ToolButtonID id)
{
	bool showOV = (id == TB_Show_VPLCOV);
	bool showLC = showOV || (id == TB_Show_VPLC);

	if (GetScrollPort  ()->IsVisible() != showLC) 
		ToggleLayerControl();
	if (GetOverviewPort()->IsVisible() != showOV) 
		ToggleOverview();
}

template <typename Func>
void VisitSiblingViewPorts(const MapControl* self, Func&& func)
{
	dms_assert(self);
	const ViewPort* vpThis = self->GetViewPort();
	if (!vpThis)
		return;

	const AbstrUnit* worldCrdUnit = vpThis->GetWorldCrdUnit();
	auto dv = self->GetDataView().lock();
	if (!dv)
		return;
	for (DataView* sv = dv->GetPrevItem(); sv && sv!=dv.get(); sv = sv->GetPrevItem())
	{
		GraphDataView* gv = dynamic_cast<GraphDataView*>(sv);
		if (!gv)
			continue;
		ViewPort* vpOther = gv->GetContents()->GetViewPort();
		if (!vpOther)
			continue;
		if (vpOther->GetWorldCrdUnit() == worldCrdUnit)
			func(self, vpOther);
	}
}

bool MapControl::OnCommand(ToolButtonID id)
{
	switch (id)
	{
		case TB_SyncScale:
			VisitSiblingViewPorts(this, [] (const MapControl* self, ViewPort* vp)->void
				{
					vp->SetCurrZoomLevel(self->GetViewPort()->GetCurrLogicalZoomLevel());
				}
			);
			return true;
		case TB_SyncROI:
			VisitSiblingViewPorts(this, [] (const MapControl* self, ViewPort* vp)->void
				{
					vp->SetROI(self->GetViewPort()->GetROI());
				}
			);
			return true;
		case TB_Show_VP:  
		case TB_Show_VPLC:
		case TB_Show_VPLCOV:
			SetLayout(id);
			return true;

		case TB_CopyLC:
			GetScrollPort()->Export();
			return true;
	}
	return base_type::OnCommand(id) || GetViewPort()->OnCommand(id);
}

CommandStatus MapControl::OnCommandEnable(ToolButtonID id) const
{
	switch (id)
	{
		case TB_SyncScale:
		case TB_SyncROI:
		{
			CommandStatus result = CommandStatus::HIDDEN;
			VisitSiblingViewPorts(this, [&result] (const MapControl* self, ViewPort* vp)->void
				{
					result = CommandStatus::ENABLED;
				}
			);
			return result;
		}
		case TB_Show_VP:  
		case TB_Show_VPLC:
		case TB_Show_VPLCOV:
		case TB_CopyLC:
			return CommandStatus::ENABLED;
	}
	return GetViewPort()->OnCommandEnable(id);
}

static TokenID s_ViewPortTokenID = GetTokenID_st("ViewPort");
static TokenID s_OverviewTokenID = GetTokenID_st("Overview");

void MapControl::Sync(TreeItem* context, ShvSyncMode sm)
{
	// Don't call GraphicContainer::Sync since that would do the ScrollPort as well and we don't want that.


	ObjectContextHandle contextHandle(context, "MapControl::Sync");

	SilentInterestRetainContext irc("MapControl::Sync");

	GetViewPort    ()->Sync( context->CreateItem(s_ViewPortTokenID), sm );
	GetOverviewPort()->Sync( context->CreateItem(s_OverviewTokenID), sm );

	dms_assert(GetViewPort    ()->GetOwner().lock().get() == this);
	dms_assert(GetOverviewPort()->GetOwner().lock().get() == this);
	dms_assert(GetScrollPort  ()->GetOwner().lock().get() == this);
	if (sm == SM_Load)
	{
		SizeT n = GetLayerSet()->NrEntries();
		if (n && !GetLayerSet()->GetActiveEntry())
			GetLayerSet()->SetActiveEntry( GetLayerSet()->GetEntry(n-1) );
	}
}

IMPL_RTTI_CLASS(MapControl)
