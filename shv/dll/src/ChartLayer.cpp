// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "ChartLayer.h"

#include "act/ActorVisitor.h"
#include "act/InvalidationBlock.h"
#include "geo/Conversions.h"
#include "geo/IsInside.h"
#include "geo/PointOrder.h"
#include "mci/Class.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "DataArray.h"
#include "DataLocks.h"

#include "AbstrController.h"
#include "DataView.h"
#include "GraphVisitor.h"
#include "LayerClass.h"
#include "LayerSet.h"
#include "MenuData.h"
#include "Theme.h"
#include "ThemeReadLocks.h"
#include "ThemeValueGetter.h"
#include "ViewPort.h"

//----------------------------------------------------------------------
// class  : ChartLayer
//----------------------------------------------------------------------

const DmsColor s_DefaultPointColor = CombineRGB(0, 128, 255);
const DmsColor s_LineColor         = CombineRGB(200, 0, 0);

ChartLayer::ChartLayer(GraphicObject* owner)
	:	base_type(owner, GetStaticClass())
{}

const AbstrUnit* ChartLayer::GetGeoCrdUnit() const
{
	auto vp = GetViewPort();
	MG_CHECK(vp);
	return vp->GetWorldCrdUnit(); // the synthetic chart-space unit
}

void ChartLayer::SetDrawMode(ChartDrawMode m)
{
	if (m_DrawMode == m)
		return;
	m_DrawMode = m;
	InvalidateDraw();
}

void ChartLayer::SetXAttr(const AbstrDataItem* xAttr)
{
	if (m_XAttr.get_ptr() == xAttr)
		return;
	m_XAttr = xAttr;
	InvalidateView(); // geometry depends on X
	InvalidateDraw();
}

ActorVisitState ChartLayer::VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const
{
	if (m_XAttr)
		if (visitor.Visit(m_XAttr.get_ptr()) == AVS_SuspendedOrFailed)
			return AVS_SuspendedOrFailed;
	return base_type::VisitSuppliers(svf, visitor);
}

void ChartLayer::DoInvalidate() const
{
	base_type::DoInvalidate();
	m_Ready = false;
	const_cast<ChartLayer*>(this)->InvalidateDraw();
}

bool ChartLayer::OnCommand(ToolButtonID id)
{
	switch (id)
	{
		case TB_ShowSelOnlyOn:  if (ShowSelectedOnlyEnabled()) SetShowSelectedOnly( true); return true;
		case TB_ShowSelOnlyOff:                                SetShowSelectedOnly(false); return true;
	}
	return base_type::OnCommand(id);
}

void ChartLayer::FillLcMenu(MenuData& menuData)
{
	// chart-kind switching (scatter <-> line <-> both) on the active series layer
	{
		SubMenu subMenu(menuData, SharedStr("Show chart as ..."));
		menuData.push_back(MenuItem(SharedStr("Points (scatter)"), make_MembFuncCmd(&ChartLayer::SetDrawModePoints), this
			, m_DrawMode == ChartDrawMode::Points ? MF_CHECKED : 0));
		menuData.push_back(MenuItem(SharedStr("Line"), make_MembFuncCmd(&ChartLayer::SetDrawModeLine), this
			, m_DrawMode == ChartDrawMode::Line ? MF_CHECKED : 0));
		menuData.push_back(MenuItem(SharedStr("Points + Line"), make_MembFuncCmd(&ChartLayer::SetDrawModePointsAndLine), this
			, m_DrawMode == ChartDrawMode::PointsAndLine ? MF_CHECKED : 0));
	}

	base_type::FillLcMenu(menuData); // Classify... + Activate, from GraphicLayer
}

void ChartLayer::DoUpdateView()
{
	m_Ready = false;

	auto theme = GetActiveTheme();
	if (!theme)
		return;
	const AbstrDataItem* yAttr = theme->GetThemeAttr();
	if (!yAttr)
		return;
	if (!PrepareDataOrUpdateViewLater(yAttr))
		return;

	const AbstrDataItem* xAttr = m_XAttr.get_ptr();
	if (xAttr && !PrepareDataOrUpdateViewLater(xAttr))
		return;

	// optional classification/palette for per-element colour
	const AbstrDataItem* breaks = theme->GetClassification();
	if (breaks && !PrepareDataOrUpdateViewLater(breaks))
		return;
	const AbstrDataItem* palette = theme->GetPaletteAttr();
	if (palette && !PrepareDataOrUpdateViewLater(palette))
		return;

	auto selTheme = GetTheme(AN_Selections);
	const AbstrDataItem* selAttr = selTheme ? selTheme->GetThemeAttr() : nullptr;
	if (selAttr && !PrepareDataOrUpdateViewLater(selAttr))
		return;

	ThemeReadLocks readLocks;
	if (!readLocks.push_back(theme.get(), DrlType::Suspendible))
	{
		readLocks.ProcessFailOrSuspend(this);
		return;
	}
	if (selTheme && !readLocks.push_back(selTheme.get(), DrlType::Suspendible))
	{
		readLocks.ProcessFailOrSuspend(this);
		return;
	}

	auto yData = yAttr->GetRefObj();
	SizeT n = yData->GetTiledRangeData()->GetElemCount();
	decltype(yData) xData;
	if (xAttr)
		xData = xAttr->GetRefObj();
	if (xData)
		MakeMin(n, xData->GetTiledRangeData()->GetElemCount());

	auto vg = theme->GetValueGetter(); // per-element classification, for colour; may be null
	const AbstrThemeValueGetter* paletteGetter = vg ? vg->CreatePaletteGetter() : nullptr;

	std::optional<DataArray<SelectionID>::locked_cseq_t> selValues;
	if (selAttr)
		selValues = const_array_cast<SelectionID>(selAttr)->GetLockedDataRead();

	m_Points.resize(n);
	m_PointColors.assign(n, s_DefaultPointColor);
	m_Selected.assign(n, false);

	bool any = false;
	Float64 minX = 0, maxX = 0, minY = 0, maxY = 0; // origin anchored at (0,0)
	for (SizeT e = 0; e != n; ++e)
	{
		Float64 x = xData ? xData->GetValueAsFloat64(e) : Float64(e);
		Float64 y = yData->GetValueAsFloat64(e);
		m_Points[e] = shp2dms_order<CrdType>(x, y); // X=col, Y=row
		if (IsDefined(x) && IsDefined(y))
		{
			if (!any) { minX = maxX = x; minY = maxY = y; any = true; }
			else { MakeMin(minX, x); MakeMax(maxX, x); MakeMin(minY, y); MakeMax(maxY, y); }
		}
		if (vg && paletteGetter && e < vg->GetCount())
			m_PointColors[e] = paletteGetter->GetColorValue(vg->GetClassIndex(e));
		if (selValues && e < selValues->size() && Bool((*selValues)[e]))
			m_Selected[e] = true;
	}
	m_Ready = true;

	// anchor origin at (0,0): the value axis only extends below zero when the data does
	SetWorldClientRect(CrdRect(
		shp2dms_order<CrdType>(Min<CrdType>(0.0, minX), Min<CrdType>(0.0, minY)),
		shp2dms_order<CrdType>(maxX, maxY)
	));

	if (!m_ZoomedOnce)
	{
		m_ZoomedOnce = true;
		auto dv = GetDataView().lock();
		auto vp = GetViewPort();
		auto ls = GetLayerSet().lock();
		// only an initial solitary layer adopts the view (see HistogramLayer)
		if (dv && vp && ls && ls->NrEntries() == 1)
			dv->PostGuiOper(
				[wvp = std::weak_ptr<GraphicObject>(vp->weak_from_this())]()
				{
					auto sharedVp = wvp.lock(); if (!sharedVp) return;
					debug_cast<ViewPort*>(sharedVp.get())->ZoomAll();
				}
			);
	}
}

bool ChartLayer::Draw(GraphDrawer& d) const
{
	if (!d.DoDrawData() || !m_Ready)
		return false;

	auto* dc = d.GetDrawContext();
	auto w2d = d.GetTransformation();
	bool showSelOnly = ShowSelectedOnly();
	DmsColor selColor = COLORREF2DmsColor(GetSelectedClr());

	auto toDev = [&w2d](CrdPoint wp) -> GPoint { return CrdPoint2GPoint(w2d.Apply(wp)); };

	// connecting polyline (in element/row order)
	if ((m_DrawMode == ChartDrawMode::Line || m_DrawMode == ChartDrawMode::PointsAndLine) && m_Points.size() >= 2)
	{
		std::vector<GPoint> dev;
		dev.reserve(m_Points.size());
		for (const auto& wp : m_Points)
			dev.push_back(toDev(wp));
		dc->DrawPolyline(dev.data(), int(dev.size()), s_LineColor, 1);
	}

	// per-element markers
	if (m_DrawMode == ChartDrawMode::Points || m_DrawMode == ChartDrawMode::PointsAndLine)
	{
		const GType r = 2; // marker half-size in device pixels
		for (SizeT e = 0, n = m_Points.size(); e != n; ++e)
		{
			if (showSelOnly && !m_Selected[e])
				continue;
			GPoint c = toDev(m_Points[e]);
			GRect marker(c.x - r, c.y - r, c.x + r + 1, c.y + r + 1);
			dc->FillRect(marker, m_Selected[e] ? selColor : m_PointColors[e]);
		}
	}

	// focus element
	SizeT fe = GetFocusElemIndex();
	if (IsDefined(fe) && fe < m_Points.size())
	{
		GPoint c = toDev(m_Points[fe]);
		GRect fr(c.x - 3, c.y - 3, c.x + 4, c.y + 4);
		dc->DrawFocusRect(fr);
	}
	return false;
}

CrdRect ChartLayer::CalcSelectedFullWorldRect() const
{
	CrdRect rect;
	if (m_Ready)
		for (SizeT e = 0, n = m_Points.size(); e != n; ++e)
			if (m_Selected[e])
				rect |= CrdRect(m_Points[e], m_Points[e]);
	return rect;
}

void ChartLayer::InvalidateFeature(SizeT featureIndex)
{
	InvalidateDraw(); // markers are tiny and cheap; redraw the whole layer
}

//----------------------------------------------------------------------
// per-element selection -> shared entity-domain selection attribute
//----------------------------------------------------------------------

void ChartLayer::WriteSelection(const std::vector<char>& picked, EventID eventID)
{
	auto selTheme = CreateSelectionsTheme();
	MG_CHECK(selTheme);

	InvalidationBlock invBlock(this);
	DataWriteLock writeLock(
		const_cast<AbstrDataItem*>(selTheme->GetThemeAttr()),
		CompoundWriteType(eventID)
	);

	bool isAdd = !(eventID & EventID::CTRLKEY);
	auto selData = mutable_array_cast<SelectionID>(writeLock.get())->GetDataWrite(no_tile, dms_rw_mode::read_write);

	SizeT n = Min<SizeT>(selData.size(), picked.size());
	bool any = false;
	for (SizeT e = 0; e != n; ++e)
		if (picked[e])
		{
			selData[e] = isAdd;
			any = true;
		}

	if (any || IsCreateNewEvent(eventID))
	{
		writeLock.Commit();
		invBlock.ProcessChange();
	}
	InvalidateDraw();
	BroadcastUpdateRequest();
}

template <typename Hit>
void ChartLayer::SelectElems(Hit&& hit, EventID eventID)
{
	if (!m_Ready)
		return;
	std::vector<char> picked(m_Points.size(), 0);
	for (SizeT e = 0, n = m_Points.size(); e != n; ++e)
		if (hit(m_Points[e]))
			picked[e] = 1;
	WriteSelection(picked, eventID);
}

void ChartLayer::SelectPoint(CrdPoint worldPnt, EventID eventID)
{
	if (!m_Ready || !(eventID & EventID::REQUEST_SEL))
		return;

	// select the single element nearest the click
	SizeT best = UNDEFINED_VALUE(SizeT);
	CrdType bestDist = MaxValue<CrdType>();
	for (SizeT e = 0, n = m_Points.size(); e != n; ++e)
	{
		CrdType dist = SqrDist<CrdType>(m_Points[e], worldPnt);
		if (dist < bestDist) { bestDist = dist; best = e; }
	}
	if (!IsDefined(best))
		return;
	std::vector<char> picked(m_Points.size(), 0);
	picked[best] = 1;
	WriteSelection(picked, eventID);
}

void ChartLayer::SelectRect(CrdRect worldRect, EventID eventID)
{
	SelectElems([worldRect](CrdPoint p) { return IsIncluding(worldRect, p); }, eventID);
}

void ChartLayer::SelectCircle(CrdPoint worldPnt, CrdType worldRadius, EventID eventID)
{
	CrdType r2 = worldRadius * worldRadius;
	SelectElems([worldPnt, r2](CrdPoint p) { return SqrDist<CrdType>(p, worldPnt) <= r2; }, eventID);
}

void ChartLayer::SelectPolygon(const CrdPoint* first, const CrdPoint* last, EventID eventID)
{
	if (first == last)
		return;
	SelectElems([first, last](CrdPoint p) { return IsInside(first, last, p); }, eventID);
}

IMPL_DYNC_LAYERCLASS(ChartLayer, ASE_SymbolColor|ASE_Selections, AN_SymbolColor, 0)
