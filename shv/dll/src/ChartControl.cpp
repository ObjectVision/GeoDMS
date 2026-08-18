// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "ChartControl.h"

#include "dbg/DebugContext.h"
#include "geom/PointOrder.h"
#include "mci/Class.h"

#include "AxisControl.h"
#include "DataView.h"
#include "LayerControl.h"
#include "LayerSet.h"
#include "ScrollPort.h"
#include "ViewPort.h"

//----------------------------------------------------------------------
// class  : ChartControl
//----------------------------------------------------------------------

const CrdType Y_AXIS_WIDTH  = 48.0;
const CrdType X_AXIS_HEIGHT = 20.0;

// Charts plot raw value ranges, which can be far smaller than the >= 10 world-units
// minimum ROI size that suits geographic coordinates (e.g. a fraction attribute in [0, 1]).
const CrdType CHART_MIN_ROI_SIZE = 1.0e-30;

ChartControl::ChartControl(DataView* dv)
	: ViewControl(dv)
{}

void ChartControl::Init(DataView* dv)
{
	assert(dv);
	m_ViewPort = make_shared_gr<ViewPort>(this, dv, "ChartView")();
	m_ViewPort->SetFitMode(FitMode::Stretch);
	m_ViewPort->SetMinRoiSize(CHART_MIN_ROI_SIZE);
	m_LayerSet = make_shared_gr<LayerSet>(m_ViewPort.get())();

	m_YAxis = make_shared_gr<AxisControl>(this, m_ViewPort.get(), AxisKind::Vertical)();
	m_XAxis = make_shared_gr<AxisControl>(this, m_ViewPort.get(), AxisKind::Horizontal)();

	m_ScrollPort = make_shared_gr<ScrollPort>(this, dv, "LayerControls", false)();
	m_LayerControlSet = make_shared_gr<LayerControlSet>(m_ScrollPort.get(), m_LayerSet.get())();

	InsertEntry(m_ScrollPort.get());
	InsertEntry(m_ViewPort.get());
	InsertEntry(m_YAxis.get());
	InsertEntry(m_XAxis.get());

	m_ViewPort  ->SetContents(m_LayerSet);
	m_ScrollPort->SetContents(m_LayerControlSet);

	dv->Activate(m_ScrollPort.get());
}

//	locate destructor here to reduce header dependencies (see MapControl)
ChartControl::~ChartControl()
{}

void ChartControl::ProcessSize(CrdPoint chartControlSize)
{
	auto plotSize = chartControlSize;

	if (m_ScrollPort->IsVisible())
	{
		TType scrollPortWidth = chartControlSize.X() / 4L;
		MakeMin(scrollPortWidth, m_ScrollPort->GetContents()->CalcFullSize().X());
		plotSize.X() -= scrollPortWidth;
	}

	CrdType plotTop    = 0;
	CrdType plotLeft   = Min<CrdType>(Y_AXIS_WIDTH,  plotSize.X());
	CrdType plotBottom = Max<CrdType>(plotSize.Y() - X_AXIS_HEIGHT, plotTop);
	CrdType plotRight  = Max<CrdType>(plotSize.X(), plotLeft);

	m_ViewPort->SetClientRect(CrdRect(shp2dms_order<CrdType>(plotLeft, plotTop ), shp2dms_order<CrdType>(plotRight, plotBottom)));
	m_YAxis   ->SetClientRect(CrdRect(shp2dms_order<CrdType>(0, plotTop        ), shp2dms_order<CrdType>(plotLeft, plotBottom)));
	m_XAxis   ->SetClientRect(CrdRect(shp2dms_order<CrdType>(plotLeft, plotBottom), shp2dms_order<CrdType>(plotRight, plotSize.Y())));

	m_ScrollPort->SetClientRect(
		CrdRect(
			shp2dms_order<CrdType>(plotSize.X(), 0),
			shp2dms_order<CrdType>(chartControlSize.X(), chartControlSize.Y())
		)
	);
}

void ChartControl::DoUpdateView()
{
	ProcessSize(GetCurrClientSize());
	ViewControl::DoUpdateView();
}

void ChartControl::ToggleLayerControl()
{
	GetScrollPort()->ToggleVisibility();

	ProcessSize(GetCurrClientSize());
}

void ChartControl::SetLayout(ToolButtonID id)
{
	bool showLC = (id == TB_Show_VPLC || id == TB_Show_VPLCOV); // charts have no Overview; treat VPLCOV as VPLC

	if (GetScrollPort()->IsVisible() != showLC)
		ToggleLayerControl();
}

bool ChartControl::OnKeyDown(UInt32 virtKey)
{
	return base_type::OnKeyDown(virtKey) || GetViewPort()->OnKeyDown(virtKey);
}

bool ChartControl::OnCommand(ToolButtonID id)
{
	switch (id)
	{
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

CommandStatus ChartControl::OnCommandEnable(ToolButtonID id) const
{
	switch (id)
	{
		case TB_Show_VP:
		case TB_Show_VPLC:
		case TB_Show_VPLCOV:
		case TB_CopyLC:
			return CommandStatus::ENABLED;

		case TB_NeedleOn:    // charts have no needle, scale bar or overview
		case TB_NeedleOff:
		case TB_ScaleBarOn:
		case TB_ScaleBarOff:
			return CommandStatus::HIDDEN;
	}
	return GetViewPort()->OnCommandEnable(id);
}

static StaticLateTokenID s_ChartViewPortTokenID("ViewPort");

void ChartControl::Sync(TreeItem* context, ShvSyncMode sm)
{
	// Don't call GraphicContainer::Sync since that would do the ScrollPort and axes as well.

	ObjectContextHandle contextHandle(context, "ChartControl::Sync");

	SilentInterestRetainContext irc("ChartControl::Sync");

	GetViewPort()->Sync( context->CreateItem(s_ChartViewPortTokenID).get(), sm );

	assert(GetViewPort  ()->GetOwner().lock().get() == this);
	assert(GetScrollPort()->GetOwner().lock().get() == this);
	if (sm == SM_Load)
	{
		// ensure the synthetic chart-space world unit exists, so layers can adopt it
		GetViewPort()->InitWorldCrdUnit(nullptr);

		SizeT n = GetLayerSet()->NrEntries();
		if (n && !GetLayerSet()->GetActiveEntry())
			GetLayerSet()->SetActiveEntry( GetLayerSet()->GetEntry(n-1) );
	}
}

IMPL_RTTI_CLASS(ChartControl)
