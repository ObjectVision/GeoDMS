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
#include "mci/ValueClass.h"

#include <algorithm>
#include <map>

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
#include "ShvUtils.h"
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

static TokenID s_DrawModeID = GetTokenID_st("ChartDrawMode");
static TokenID s_XAttrID    = GetTokenID_st("XAttr");

void ChartLayer::Sync(TreeItem* viewContext, ShvSyncMode sm)
{
	base_type::Sync(viewContext, sm); // GraphicLayer: themes, visibility, show-sel-only
	SyncRef(m_XAttr, viewContext, s_XAttrID, sm); // persist the chosen X attribute by reference
	if (sm == SM_Save)
		SaveValue<UInt32>(viewContext, s_DrawModeID, UInt32(m_DrawMode));
	else
	{
		m_DrawMode = ChartDrawMode(LoadValue<UInt32>(viewContext, s_DrawModeID, UInt32(ChartDrawMode::Points)));
		// a layer reconstructed from a saved desktop must respect the restored ROI: suppress the
		// one-shot auto-ZoomAll that a freshly-added layer posts from DoUpdateView.
		m_ZoomedOnce = true;
	}
}

void ChartLayer::SetXAttr(const AbstrDataItem* xAttr)
{
	if (m_XAttr.get_ptr() == xAttr)
		return;
	m_XAttr = xAttr;
	m_ZoomPending = true; // the world extent changes with X; re-fit once the new geometry is ready
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
		menuData.push_back(MenuItem(SharedStr("Bars"), make_MembFuncCmd(&ChartLayer::SetDrawModeBars), this
			, m_DrawMode == ChartDrawMode::Bars ? MF_CHECKED : 0));
	}

	AddXAxisMenu(menuData); // "Use as X axis ..." picker

	base_type::FillLcMenu(menuData); // Classify... + Activate, from GraphicLayer
}

void ChartLayer::AddXAxisMenu(MenuData& menuData)
{
	auto theme = GetActiveTheme();
	const AbstrDataItem* yAttr = theme ? theme->GetThemeAttr() : nullptr;
	if (!yAttr)
		return;
	const AbstrUnit* domain = yAttr->GetAbstrDomainUnit(); // the entity domain E

	SubMenu subMenu(menuData, SharedStr("Use as X axis ..."));

	// default: the element's row number (id of E)
	menuData.push_back(MenuItem(SharedStr("Row number (default)")
		, make_MembFuncCmd(&ChartLayer::SetXAttr, static_cast<const AbstrDataItem*>(nullptr)), this
		, m_XAttr.is_null() ? MF_CHECKED : 0));

	// candidate X axes: sibling attributes of Y that share E as their domain
	auto container = yAttr->GetTreeParent();
	if (container)
	{
		for (auto subItem = container->GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
		{
			auto candidate = AsDynamicDataItem(subItem);
			if (!candidate || candidate == yAttr)
				continue;
			if (candidate->GetAbstrDomainUnit() != domain)
				continue;
			menuData.push_back(MenuItem(SharedStr(candidate->GetName())
				, make_MembFuncCmd(&ChartLayer::SetXAttr, static_cast<const AbstrDataItem*>(candidate)), this
				, m_XAttr.get_ptr() == candidate ? MF_CHECKED : 0));
		}
	}
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

	// A non-numeric X attribute (e.g. a string or relation) is laid out categorically:
	// distinct values map to consecutive ordinals 0,1,2,… and feed the axis tick labels.
	bool xIsCategorical = xAttr && !xAttr->GetAbstrValuesUnit()->GetValueType()->IsNumeric();

	auto vg = theme->GetValueGetter(); // per-element classification, for colour; may be null
	const AbstrThemeValueGetter* paletteGetter = vg ? vg->CreatePaletteGetter() : nullptr;

	std::optional<DataArray<SelectionID>::locked_cseq_t> selValues;
	if (selAttr)
		selValues = const_array_cast<SelectionID>(selAttr)->GetLockedDataRead();

	m_Points.resize(n);
	m_PointColors.assign(n, s_DefaultPointColor);
	m_Selected.assign(n, false);
	m_XAxisLabels.clear();

	// categorical ordinal assignment, in order of first appearance
	std::map<SharedStr, CrdType> categoryOrdinals;
	GuiReadLock categoryLock;

	bool any = false;
	Float64 minX = 0, maxX = 0, minY = 0, maxY = 0; // origin anchored at (0,0)
	for (SizeT e = 0; e != n; ++e)
	{
		Float64 x;
		if (xIsCategorical)
		{
			SharedStr label = xData->AsString(e, categoryLock, FormattingFlags::None);
			auto it = categoryOrdinals.find(label);
			if (it == categoryOrdinals.end())
			{
				CrdType ordinal = CrdType(categoryOrdinals.size());
				it = categoryOrdinals.emplace(label, ordinal).first;
				m_XAxisLabels.emplace_back(ordinal, label);
			}
			x = it->second;
		}
		else
			x = xData ? xData->GetValueAsFloat64(e) : Float64(e);

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

	// bar half-width: 0.4× the smallest gap between distinct X positions (so adjacent bars
	// nearly touch without overlapping); categorical/row-number X have unit spacing.
	{
		CrdType minGap = 1.0;
		if (!xIsCategorical && xData && n >= 2)
		{
			std::vector<CrdType> xs(n);
			for (SizeT e = 0; e != n; ++e) xs[e] = m_Points[e].X();
			std::sort(xs.begin(), xs.end());
			bool gapFound = false;
			for (SizeT e = 1; e != n; ++e)
			{
				CrdType g = xs[e] - xs[e - 1];
				if (g > 0 && (!gapFound || g < minGap)) { minGap = g; gapFound = true; }
			}
		}
		m_BarHalfWidth = 0.4 * minGap;
	}

	// anchor origin at (0,0): the value axis only extends below zero when the data does.
	// Pad the X-extent by a bar half-width so the outermost bars are not clipped.
	CrdType xPad = (m_DrawMode == ChartDrawMode::Bars) ? m_BarHalfWidth : 0.0;
	SetWorldClientRect(CrdRect(
		shp2dms_order<CrdType>(Min<CrdType>(0.0, minX) - xPad, Min<CrdType>(0.0, minY)),
		shp2dms_order<CrdType>(maxX + xPad, maxY)
	));

	if (!m_ZoomedOnce || m_ZoomPending)
	{
		bool forceZoom = m_ZoomPending; // an explicit X-axis change re-fits regardless of layer count
		m_ZoomedOnce = true;
		m_ZoomPending = false;
		auto dv = GetDataView().lock();
		auto vp = GetViewPort();
		auto ls = GetLayerSet().lock();
		// an initial solitary layer adopts the view (see HistogramLayer); an X-axis change always re-fits
		if (dv && vp && ls && (forceZoom || ls->NrEntries() == 1))
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

	// vertical bars: one filled rect per element, from y=0 to y=value, centred at x
	if (m_DrawMode == ChartDrawMode::Bars)
	{
		DmsColor frameColor = CombineRGB(64, 64, 64);
		for (SizeT e = 0, n = m_Points.size(); e != n; ++e)
		{
			if (showSelOnly && !m_Selected[e])
				continue;
			CrdType x = m_Points[e].X(), y = m_Points[e].Y(); // X=col, Y=row(value)
			CrdRect barWorld(
				shp2dms_order<CrdType>(x - m_BarHalfWidth, Min<CrdType>(0.0, y)),
				shp2dms_order<CrdType>(x + m_BarHalfWidth, Max<CrdType>(0.0, y))
			);
			w2d.InplApply(barWorld);
			GRect bar = CrdRect2GRect(barWorld);
			dc->FillRect(bar, m_Selected[e] ? selColor : m_PointColors[e]);
			if (bar.right - bar.left >= 3) // outline only when wide enough to read
			{
				GPoint outline[5] = {
					GPoint(bar.left, bar.top), GPoint(bar.right-1, bar.top),
					GPoint(bar.right-1, bar.bottom-1), GPoint(bar.left, bar.bottom-1), GPoint(bar.left, bar.top)
				};
				dc->DrawPolyline(outline, 5, frameColor, 1);
			}
		}
		return false;
	}

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
