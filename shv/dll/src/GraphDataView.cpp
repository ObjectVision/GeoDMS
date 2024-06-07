// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#include <memory>
#include <ppltasks.h>

#include "GraphDataView.h"

#include "mci/Class.h"
#include "dbg/DmsCatch.h"

#include "AbstrUnit.h"
#include "OperationContext.h"
#include "Projection.h"
#include "PropFuncs.h"
#include "TreeItemProps.h"

#include "AbstrCmd.h"
#include "GraphicRect.h"
#include "LayerInfo.h"
#include "LayerClass.h"
#include "LayerSet.h"
#include "MapControl.h"
#include "LayerControl.h"
#include "ScrollPort.h"
#include "Theme.h"
#include "ViewPort.h"
#include "WmsLayer.h"

/////////////////////////////////////////////////////////////////////////////
// GraphDataView Implementation


GraphDataView::GraphDataView(TreeItem* viewContext, ShvSyncMode sm)
:	DataView(viewContext)
{}

std::shared_ptr<MapControl> GraphDataView::GetContents()       
{
	assert(m_Contents);
	return debug_pointer_cast<MapControl>(m_Contents);
}

std::shared_ptr<const MapControl> GraphDataView::GetContents() const
{
	dms_assert(m_Contents); 
	return debug_pointer_cast<const MapControl>(m_Contents);
}

#include "Metric.h"

bool CompatibleCrds(const AbstrUnit* a, const AbstrUnit* b)
{
	assert(a);
	if (!b)
		return true;

redo_a:
	if (auto ap = a->GetProjection())
		if (auto apb = ap->GetBaseUnit())
			if (a != apb)
			{
				a = apb;
				goto redo_a;
			}

redo_b:
	if (auto bp = b->GetProjection())
		if (auto bpb = bp->GetBaseUnit())
		{
			b = bpb;
			goto redo_b;
		}

	// Callers guarantee that stuff came from GetWorldCrdUnit that went all the way to the last object
	assert(a->m_State.GetProgress() >= PS_MetaInfo);
	assert(b->m_State.GetProgress() >= PS_MetaInfo);

	if (a == b)
		return true;

	if (!a->IsCacheItem())
		a = AsUnit(a->GetUltimateItem());

	if (!b->IsCacheItem())
		b = AsUnit(b->GetUltimateItem());

	return a == b;
}

// resolve invisible covariant return type issue in header
bool GraphDataView::CanContain(const TreeItem* viewCandidate) const
{
	if (!IsDataItem(viewCandidate))
		return false;

	LayerInfo res = GetLayerInfo(AsDynamicDataItem(viewCandidate));
	if (!res.IsComplete())
		return false;
	assert(!res.IsAspect());

	SharedUnit resCrdUnit = res.GetWorldCrdUnit();
	SharedUnit vpCrdUnit = GetContents()->GetViewPort()->GetWorldCrdUnit();

	return CompatibleCrds(resCrdUnit.get(), vpCrdUnit.get());
}

class AddLayerCmd : public AbstrCmd
{
	typedef AbstrCmd base_type;
public:
	AddLayerCmd(const AbstrDataItem* viewItem, const LayerInfo& info, bool isDropped)
		:	m_ViewItem(viewItem)
		,	m_LayerInfo(info)
		,	m_IsDropped(isDropped)
	{}
	GraphVisitState DoLayerSet(LayerSet* ls) override
	{
		auto dv = ls->GetDataView().lock(); if (!dv) return GVS_Handled;
		if (!m_WorldCrdUnit) // could be NULL when filling Overview Layerset
		{
			m_Result = ls->CreateLayer(m_ViewItem, m_LayerInfo, dv.get(), true);
			if (!m_Result) 
			{
				m_ViewItem = nullptr; // only show once
				return GVS_UnHandled;
			}
			if (ls->NrEntries() == 0) // first layer in first ViewPort? prepare adding topographic layers
			{
				m_WorldCrdUnit = m_Result->GetWorldCrdUnit(); // find and add topographic layers 
				dms_assert(m_WorldCrdUnit);
			}
		}

		bool layerWasAdded = false;

		if (m_WorldCrdUnit) // add topographic layer(s) to viewport and to overview if this is the first layer in view
		{
			std::shared_ptr<LayerSet>     defaultLayerSet;
			std::shared_ptr<GraphicLayer> singletonTopoLayer;

			SharedStr dd = m_WorldCrdUnit->GetBackgroundReference();
			CharPtr
				i = dd.begin(), 
				e = dd.send();
			while (i != e)
			{
				try {
					MG_CHECK(m_Result);
					auto geoCrdUnitContext = m_Result->GetGeoCrdUnit();
					geoCrdUnitContext = AsUnit(geoCrdUnitContext->GetUltimateSourceItem());

					MG_CHECK(geoCrdUnitContext);
					const TreeItem* topographicItem = GetNextDialogDataRef(geoCrdUnitContext, i, e);
					if (!topographicItem || topographicItem == m_ViewItem)
						continue;
					LayerSet* currSet = defaultLayerSet ? defaultLayerSet.get() : ls;
					std::shared_ptr<GraphicLayer> topoLayer;
					if (IsDataItem(topographicItem)) {
						LayerInfo topoLayerInfo = GetLayerInfo(AsDataItem(topographicItem));
						if (topoLayerInfo.IsComplete()) {
							topoLayer = currSet->CreateLayer(AsDataItem(topographicItem), topoLayerInfo, dv.get(), false);
						}
					}
					else if (topographicItem->GetConstSubTreeItemByID(GetTokenID_mt("layer"))) // Layer is necessary in the geodms layer file
					{
						auto newLayer = std::make_shared<WmsLayer>(currSet);
						newLayer->SetWorldCrdUnit(m_WorldCrdUnit);

						topoLayer = newLayer;
						newLayer->SetSpecContainer(topographicItem);
					}
					if (topoLayer)
						layerWasAdded = AddTopoLayer(topoLayer, ls, defaultLayerSet, singletonTopoLayer);
				}
				catch (...)
				{
					catchAndReportException();
				}
			}
			if (singletonTopoLayer)
			{
				dms_assert(!defaultLayerSet);
				ls->InsertEntry(singletonTopoLayer.get());
			}
		}

		if (m_ViewItem) // could be NULL when filling Overview Layerset
		{
			dms_assert(m_Result);

			// check for helper items referred at by m_ViewItem.
			SharedStr dd = TreeItem_GetDialogData(m_ViewItem);
			CharPtr
				i = dd.begin(), 
				e = dd.send();
			while (i != e)
			{
				const TreeItem* helperItem = GetNextDialogDataRef(m_ViewItem, i, e);
				if (helperItem && helperItem != m_ViewItem && IsDataItem(helperItem))
				{
					LayerInfo layerInfo = GetLayerInfo(AsDataItem(helperItem));
					if (layerInfo.IsComplete())
					{
						std::shared_ptr<GraphicLayer> helperLayer = ls->CreateLayer(AsDataItem(helperItem), layerInfo, dv.get(), true);
						if (helperLayer)
							ls->InsertEntry(helperLayer.get());
					}
				}
			}

			m_ViewItem = nullptr; // only show in one ViewPort

			// finally add the created thematic layer.
			ls->InsertEntry(m_Result.get());
			ls->SetActiveEntry(m_Result.get());

			layerWasAdded = true;
			if	(	m_Result->GetActiveTheme()
				&&	m_Result->GetActiveTheme()->GetPaletteDomain()
				)
			{
				dms_assert(m_Result->DetailsVisible());

				SharedPtr<const AbstrUnit> paletteDomain = m_Result->GetActiveTheme()->GetPaletteDomain();

				//TODO: Dit moet toch handiger kunnen 
				dms_assert(!SuspendTrigger::DidSuspend());
				if (IsMultiThreaded2())
				{
					SuspendTrigger::DoSuspend();
					paletteDomain->PrepareData(); // wants to know GetCount();
					assert(IsCalculatingOrReady(paletteDomain->GetCurrRangeItem()));
					SuspendTrigger::Resume();
				}
				else
				{
					SuspendTrigger::SilentBlocker xx("AddLayerCmd::PreparePaletteDomain()");
					paletteDomain->PrepareData();
				}
#if defined(MG_DEBUG)
				auto* ultimateCU = AsUnit(paletteDomain->GetCurrRangeItem());
				dbg_assert(ultimateCU->CheckMetaInfoReadyOrPassor());
				dbg_assert(CheckCalculatingOrReady(ultimateCU) || ultimateCU->WasFailed(FR_Data));
#endif
				std::weak_ptr<GraphicLayer> wResLayer = m_Result;
				std::weak_ptr<LayerSet> wLayerSet = ls->weak_from_base<LayerSet>();
				std::weak_ptr<DataView> wDv = ls->GetDataView();

				TimeStamp tsActive = UpdateMarker::GetActiveTS(MG_DEBUG_TS_SOURCE_CODE("Obtaining active frame for paletteDomain counting job"));

				auto determineDetailsVisible =
					[wResLayer, wLayerSet, paletteDomain, wDv, tsActive]()->void {
						try {

							UpdateMarker::ChangeSourceLock tsLock(tsActive, "paletteDomain.counting");

							SizeT count = paletteDomain->GetCount();
							auto sDv = wDv.lock(); if (!sDv) return;
							sDv->AddGuiOper([count, wResLayer, wLayerSet]() {
								auto sResLayer = wResLayer.lock();
								if (!sResLayer)
									return;
								sResLayer->SetDetailsTooLong(count > 32);
								if (sResLayer->DetailsTooLong() == sResLayer->DetailsVisible())
									sResLayer->ToggleDetailsVisibility(); // make palette invisible iff too long details
							});
						}
						catch (...) {} // let it go, it's just GUI.
					};
				if (IsMultiThreaded2())
				{
					auto t = dms_task(determineDetailsVisible);
				}
				else determineDetailsVisible();
			}
		}
		return layerWasAdded ? GVS_Handled: GVS_UnHandled;
	}

	bool AddTopoLayer(std::shared_ptr<GraphicLayer> topoLayer, LayerSet* ls
	,	std::shared_ptr<LayerSet>&      defaultLayerSet
	,	std::shared_ptr<GraphicLayer>& singletonTopoLayer
	)
	{
		if (!defaultLayerSet)
		{
			if (singletonTopoLayer)
			{
				defaultLayerSet = ls->CreateSubset("Background");
				defaultLayerSet->ToggleDetailsVisibility();
				defaultLayerSet->SetTopographic();
				ls->InsertEntry(defaultLayerSet.get());

				singletonTopoLayer->ClearOwner();
				singletonTopoLayer->SetOwner(defaultLayerSet.get());
				defaultLayerSet->InsertEntry(singletonTopoLayer.get());

				singletonTopoLayer = nullptr;

				topoLayer->ClearOwner();
				topoLayer->SetOwner(defaultLayerSet.get());
				defaultLayerSet->InsertEntry(topoLayer.get());

			}
			else
				singletonTopoLayer = topoLayer;
		}
		else
		{
			defaultLayerSet->InsertEntry(topoLayer.get());
			if (defaultLayerSet->NrEntries() > 6)
				defaultLayerSet->SetDetailsTooLong(true);
		}
		return true;
	}

	GraphVisitState DoViewPort(ViewPort* vp) override
	{
		dms_assert(!SuspendTrigger::DidSuspend());
		MG_DEBUG_DATA_CODE( SuspendTrigger::ApplyLock findIt; )

		dms_assert(vp);
		// use Invite to jump over the Visibility filter in the Visit method
		if (vp->GetContents()->InviteGraphVistor(*this) == GVS_Handled) // layer was added in component of this viewPort
		{
			if (!vp->GetWorldCrdUnit())
			{
				dms_assert(m_Result);
				dms_assert(m_WorldCrdUnit);
				vp->InitWorldCrdUnit(m_WorldCrdUnit);
				vp->AL_ZoomAll();
			}
			return GVS_Handled;
		}
		return GVS_UnHandled;
	}
	GraphVisitState DoMapControl(MapControl* mc) override
	{
		if (!m_IsDropped) {
			LayerControlSet* lcs = mc->GetLayerControls();
			if (lcs) {
				for (gr_elem_index i = 0, n = lcs->NrEntries(); i != n; ++i) {
					LayerControl* lc = dynamic_cast<LayerControl*>(lcs->GetEntry(i));
					if (lc) {
						GraphicLayer* gl = lc->GetLayer();
						if (gl && gl->GetActiveAttr() == m_ViewItem) {
							auto dv = mc->GetDataView().lock();
							if (!dv) 
								break;
							dv->Activate(lc);
							return GVS_Handled;
						}
					}
				}
			}
		}
		// use Invite to jump over the Visibility filter in the Visit method
		if (mc->GetViewPort()->InviteGraphVistor(*this) == GVS_UnHandled) // add layer to main viewport
			return GVS_UnHandled;

		if (m_WorldCrdUnit) // is topographicItem found for insertion in overview or can we set world coords for overview?
		{
			GraphicObject* overview = mc->GetOverviewPort();

			// use Invite to jump over the Visibility filter in the Visit method
			if (overview->InviteGraphVistor(*this) == GVS_Handled)
			{
				if (!overview->IsVisible()) mc->ToggleOverview(); // make overview visible 

				auto graphRect = make_shared_gr<GraphicRect>(mc->GetOverviewLayers())();

				mc->GetOverviewLayers()->InsertEntry(graphRect.get());
			}
		}
		if (m_Result)
			mc->GetScrollPort()->ScrollHome();

		return GVS_Handled;
	}

private:
	const AbstrDataItem*    m_ViewItem;
	LayerInfo               m_LayerInfo;
	std::shared_ptr<GraphicLayer> m_Result;
	const AbstrUnit*        m_WorldCrdUnit = nullptr;
	bool                    m_IsDropped;
};

void GraphDataView::AddLayer(const TreeItem* viewItem, bool isDropped)
{
	assert(!SuspendTrigger::DidSuspend());
	MG_USERCHECK(IsDataItem(viewItem));

	LayerInfo info = GetCompleteLayerInfoOrThrow(viewItem);
	assert(!SuspendTrigger::DidSuspend());

	MG_USERCHECK(info.IsComplete());
	AddLayerCmd(AsDynamicDataItem(viewItem), info, isDropped).Visit(GetContents().get());
}

ExportInfo GraphDataView::GetExportInfo()
{
	return GetContents()->GetViewPort()->GetExportInfo();
}

SharedStr GraphDataView::GetCaption() const // Mapview caption
{
	auto mapContents = GetContents();
	if (mapContents)
	{
		SharedStr spatialRefStr;
		auto wcu = mapContents->GetViewPort()->GetWorldCrdUnit();
		if (wcu)
		{
			auto world_crd_unit_label_property = TreeItemPropertyValue(wcu, labelPropDefPtr);
			if (!world_crd_unit_label_property.empty()) // prioritize label over srs def in mapview caption
				spatialRefStr = world_crd_unit_label_property;
			else
				spatialRefStr = wcu->GetSpatialReference();
		}

		if (spatialRefStr.empty())
			spatialRefStr = "";
		else
			spatialRefStr = "with " + spatialRefStr;

		if (auto ls = mapContents->GetLayerSet())
			if (auto active_layer = ls->GetActiveLayer())
				return spatialRefStr + ", " + active_layer->GetCaption();

		return spatialRefStr;
	}
	return SharedStr("");
}

LayerInfo GraphDataView::GetCompleteLayerInfoOrThrow(const TreeItem* viewItem) const
{
	dms_assert(viewItem);
	if (!IsDataItem(viewItem))
		viewItem->throwItemError("Cannot Display this as Layer because it is not a DataItem");

	LayerInfo res = GetLayerInfo(AsDynamicDataItem(viewItem));
	if (!res.IsComplete())
		viewItem->throwItemErrorF("Cannot Display this as Layer because %s", res.m_Descr.c_str());
	return res;
}

IMPL_RTTI_CLASS(GraphDataView);
