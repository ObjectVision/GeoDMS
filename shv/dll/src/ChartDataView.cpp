// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "ChartDataView.h"

#include "mci/Class.h"
#include "mci/ValueClass.h"

#include "AbstrDataItem.h"
#include "AbstrUnit.h"

#include "ChartControl.h"
#include "HistogramLayer.h"
#include "LayerControl.h"
#include "LayerInfo.h"
#include "LayerSet.h"
#include "ScrollPort.h"
#include "Theme.h"
#include "ViewPort.h"

/////////////////////////////////////////////////////////////////////////////
// ChartDataView Implementation

ChartDataView::ChartDataView(TreeItem* viewContext, ShvSyncMode sm)
	:	DataView(viewContext)
{}

std::shared_ptr<ChartControl> ChartDataView::GetContents()
{
	assert(m_Contents);
	return debug_pointer_cast<ChartControl>(m_Contents);
}

std::shared_ptr<const ChartControl> ChartDataView::GetContents() const
{
	assert(m_Contents);
	return debug_pointer_cast<const ChartControl>(m_Contents);
}

bool ChartDataView::CanContain(const TreeItem* viewCandidate) const
{
	if (!IsDataItem(viewCandidate))
		return false;

	auto adi = AsDataItem(viewCandidate);
	if (adi->HasVoidDomainGuarantee())
		return false; // parameters have no row set to aggregate over

	return adi->GetAbstrValuesUnit()->GetValueType()->IsNumericOrBool();
}

void ChartDataView::AddLayer(const TreeItem* viewItem, bool isDropped)
{
	SuspendTrigger::Resume(); // drop events arrive with a possibly suspended trigger (unlike createView, which Resumes)

	MG_USERCHECK(IsDataItem(viewItem));
	if (!CanContain(viewItem))
		viewItem->throwItemError("Cannot show this as Histogram Chart because it is not a numeric attribute");

	auto adi = AsDynamicDataItem(viewItem);
	auto chartControl = GetContents();
	auto vp = chartControl->GetViewPort();
	auto ls = chartControl->GetLayerSet();

	vp->InitWorldCrdUnit(nullptr); // ensure the synthetic chart-space world unit exists

	auto layer = std::make_shared<HistogramLayer>(ls);

	// theme context for classification/palette discovery or default-break creation
	LayerInfo aspectContext(LayerInfo::Aspect, nullptr, nullptr, adi, HistogramLayer::GetStaticClass());

	auto theme = Theme::Create(AN_BrushColor, adi, aspectContext, this, true);
	assert(theme);
	layer->SetThemeAndActivate(theme.get(), adi);
	layer->ConnectSelectionsTheme(this);

	ls->InsertEntry(layer.get());
	ls->SetActiveEntry(layer.get());

	if (vp->GetROI().empty()) // first layer: bootstrap the layer's first DoUpdateView; zooms once bins are known
		vp->AL_ZoomAll();
	else
	{
		// dropped into a live chart: schedule the layer's first DoUpdateView on the update timer.
		// A synchronous pull here would deadlock on the classification task that Theme::Create
		// queued for this same (main) thread, which still holds its palette-domain write lock.
		layer->InvalidateView();
		RequestUpdate();
	}

	chartControl->GetScrollPort()->ScrollHome();
}

ExportInfo ChartDataView::GetExportInfo()
{
	return GetContents()->GetViewPort()->GetExportInfo();
}

SharedStr ChartDataView::GetCaption() const
{
	auto chartContents = GetContents();
	if (chartContents)
		if (auto ls = chartContents->GetLayerSet())
			if (auto active_layer = ls->GetActiveLayer())
				return SharedStr("Chart of ") + active_layer->GetThemeDisplayNameWithinContext(chartContents.get(), false);
	return SharedStr("Chart");
}

const TreeItem* ChartDataView::GetCaptionItem() const // #418: the active layer's domain, as a cross-view disambiguation peer
{
	auto chartContents = GetContents();
	if (chartContents)
		if (auto ls = chartContents->GetLayerSet())
			if (auto active_layer = ls->GetActiveLayer())
				return active_layer->GetActiveEntity();
	return nullptr;
}

IMPL_RTTI_CLASS(ChartDataView);
