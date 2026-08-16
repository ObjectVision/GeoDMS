// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "ChartDataView.h"

#include "mci/Class.h"
#include "mci/ValueClass.h"

#include "AbstrDataItem.h"
#include "AbstrUnit.h"
#include "PropFuncs.h"

#include "ChartControl.h"
#include "ChartLayer.h"
#include "GraphicLayer.h"
#include "HistogramLayer.h"
#include "LayerClass.h"
#include "LayerControl.h"
#include "LayerInfo.h"
#include "LayerSet.h"
#include "ScrollPort.h"
#include "ShvUtils.h"
#include "Theme.h"
#include "ViewPort.h"

//----------------------------------------------------------------------
// ChartKind <-> view-context DialogType token (issue #75)
//----------------------------------------------------------------------

static TokenID s_ChartScatterID = GetTokenID_st("chart_scatter");
static TokenID s_ChartLineID    = GetTokenID_st("chart_line");
static TokenID s_ChartBarID     = GetTokenID_st("chart_bar");

void SetViewContextChartKind(TreeItem* viewContext, ChartKind kind)
{
	assert(viewContext);
	switch (kind) {
		case ChartKind::Scatter: TreeItem_SetDialogType(viewContext, s_ChartScatterID); break;
		case ChartKind::Line:    TreeItem_SetDialogType(viewContext, s_ChartLineID);    break;
		case ChartKind::Bar:     TreeItem_SetDialogType(viewContext, s_ChartBarID);     break;
		default: break; // Histogram is the default; no token needed
	}
}

ChartKind GetViewContextChartKind(const TreeItem* viewContext)
{
	if (!viewContext)
		return ChartKind::Histogram;
	TokenID dt = TreeItem_GetDialogType(viewContext);
	if (dt == s_ChartScatterID) return ChartKind::Scatter;
	if (dt == s_ChartLineID)    return ChartKind::Line;
	if (dt == s_ChartBarID)     return ChartKind::Bar;
	return ChartKind::Histogram;
}

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
		viewItem->throwItemError("Cannot show this in a Chart because it is not a numeric attribute");

	auto adi = AsDynamicDataItem(viewItem);

	ChartKind kind = GetViewContextChartKind(GetViewContext());
	if (kind == ChartKind::Histogram)
		AddHistogramLayer(adi, isDropped);
	else
		AddSeriesLayer(adi, kind, isDropped);
}

void ChartDataView::AddHistogramLayer(const AbstrDataItem* adi, bool isDropped)
{
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

	ActivateAndZoom(layer.get(), isDropped);
	chartControl->GetScrollPort()->ScrollHome();
}

void ChartDataView::AddSeriesLayer(const AbstrDataItem* adi, ChartKind kind, bool isDropped)
{
	auto chartControl = GetContents();
	auto vp = chartControl->GetViewPort();
	auto ls = chartControl->GetLayerSet();

	vp->InitWorldCrdUnit(nullptr); // ensure the synthetic chart-space world unit exists

	// A ChartLayer takes the X and Y axes as separate attributes: Y = the (classified) value
	// shown on the vertical axis, X = the row number (id of E) by default, settable later for
	// X-axis management. Scatter and line differ only by draw mode, so switching is a toggle.
	auto layer = std::make_shared<ChartLayer>(ls);
	layer->SetDrawMode(
		kind == ChartKind::Bar  ? ChartDrawMode::Bars :
		kind == ChartKind::Line ? ChartDrawMode::Line :
		                          ChartDrawMode::Points);

	// thematic symbolisation (issue #75 requirement): a classification + colour palette of the
	// value attribute drives the per-element colour (AN_SymbolColor) and brings the "Classify..."
	// management menu. The theme's thematic attr is also the source of the Y values.
	LayerInfo aspectContext(LayerInfo::Aspect, nullptr, nullptr, adi, ChartLayer::GetStaticClass());
	auto theme = Theme::Create(AN_SymbolColor, adi, aspectContext, this, true);
	MG_CHECK(theme);
	layer->SetThemeAndActivate(theme.get(), adi);
	layer->ConnectSelectionsTheme(this);

	ls->InsertEntry(layer.get());
	ls->SetActiveEntry(layer.get());

	ActivateAndZoom(layer.get(), isDropped);
	chartControl->GetScrollPort()->ScrollHome();
}

void ChartDataView::ActivateAndZoom(GraphicLayer* layer, bool isDropped)
{
	auto vp = GetContents()->GetViewPort();
	if (vp->GetROI().empty()) // first layer: bootstrap the layer's first DoUpdateView; zooms once data is ready
		vp->AL_ZoomAll();
	else
	{
		// dropped into a live chart: schedule the layer's first DoUpdateView on the update timer.
		// A synchronous pull here can deadlock on a classification/calc task queued for this same
		// (main) thread that still holds a write lock.
		layer->InvalidateView();
		RequestUpdate();
	}
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
