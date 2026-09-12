// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "PieLayer.h"

#include "ChartGeometry.h"
#include "InvalidationBlock.h"
#include "vt/Conversions.h"
#include "geom/PointOrder.h"
#include "mci/Class.h"
#include "ser/AsString.h"
#include "utl/StrFormat.h"

#include <algorithm>
#include <cmath>

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "DataArray.h"
#include "DataItemClass.h"
#include "DataLocks.h"
#include "StateChangeNotification.h"
#include "Unit.h"
#include "UnitClass.h"

#include "AbstrCmd.h"
#include "AbstrController.h"
#include "DataItemColumn.h"
#include "DataView.h"
#include "GraphVisitor.h"
#include "LayerClass.h"
#include "LayerSet.h"
#include "MenuData.h"
#include "PaletteControl.h"
#include "ShvUtils.h"
#include "Theme.h"
#include "ThemeReadLocks.h"
#include "ThemeValueGetter.h"
#include "ViewPort.h"

//----------------------------------------------------------------------
// class  : PieLayer
//----------------------------------------------------------------------

const DmsColor s_DefaultSliceColor = CombineRGB(0, 128, 255);
const DmsColor s_SliceFrameColor   = CombineRGB(64, 64, 64);

// The disc has radius 1 around the origin of chart space; the layer's world rect adds a margin
// around it, so that the outline is not drawn on the edge of the plot area.
const CrdType PIE_RADIUS      = 1.0;
const CrdType PIE_WORLD_EXTENT = 1.05;
const CrdType DONUT_HOLE       = 0.5;  // the hole's radius as a fraction of the innermost ring's width
const Float64 PI = 3.14159265358979323846;

// One arc vertex per this many device pixels: fine enough for a round outline, and a slice too
// thin to show gets two vertices, not a hundred.
const CrdType ARC_PIXELS_PER_VERTEX = 3.0;
// A slice's outline is only drawn when its arc is at least this wide, as ChartLayer frames only
// bars wide enough to read; below that the outlines would paint over the slices themselves.
const CrdType MIN_FRAMED_ARC_PIXELS = 3.0;
// Slices are sampled at this angular resolution for the hit tests of the selection tools.
const CrdType HIT_TEST_SEGMENT_FRACTION = 1.0 / 72.0; // five degrees

PieLayer::PieLayer(GraphicObject* owner)
	:	base_type(owner, GetStaticClass())
{
	// The value attribute is the layer's subject: it rides the AN_Feature theme, as a feature
	// layer's geometry does, and ThemeSet::SyncThemes leaves the active theme alone when it
	// reloads a layer that was saved with AN_Feature active, so it is set here once and for all.
	SetActiveTheme(AN_Feature);
}

const AbstrDataItem* PieLayer::GetValueAttr() const
{
	auto featureTheme = GetTheme(AN_Feature);
	return featureTheme ? featureTheme->GetPaletteAttr() : nullptr;
}

const AbstrUnit* PieLayer::GetGeoCrdUnit() const
{
	auto vp = GetViewPort();
	MG_CHECK(vp);
	return vp->GetWorldCrdUnit(); // the synthetic chart-space unit
}

void PieLayer::SetDrawMode(PieDrawMode m)
{
	if (m_DrawMode == m)
		return;
	m_DrawMode = m;
	InvalidateDraw();

	// #1238: the view caption names the draw mode ("Pie Chart of ...", "Donut Chart of ..."), so a
	// toggle from this layer's popup menu has to re-emit it.
	if (IsActive())
		if (auto dv = GetDataView().lock())
			dv->OnCaptionChanged();
}

static StaticLateTokenID s_PieDrawModeID("PieDrawMode");

void PieLayer::Sync(TreeItem* viewContext, ShvSyncMode sm)
{
	base_type::Sync(viewContext, sm); // GraphicLayer: themes, visibility, show-sel-only
	if (sm == SM_Save)
		SaveValue<UInt32>(viewContext, s_PieDrawModeID, UInt32(m_DrawMode));
	else
	{
		m_DrawMode = PieDrawMode(LoadValue<UInt32>(viewContext, s_PieDrawModeID, UInt32(PieDrawMode::Pie)));
		// a layer reconstructed from a saved desktop must respect the restored ROI: suppress the
		// one-shot auto-ZoomAll that a freshly-added layer posts from DoUpdateView.
		m_ZoomedOnce = true;
	}
}

void PieLayer::DoInvalidate() const
{
	base_type::DoInvalidate();
	m_Ready = false;
	const_cast<PieLayer*>(this)->InvalidateDraw();
}

void PieLayer::InvalidateFeature(SizeT featureIndex)
{
	InvalidateDraw(); // a slice's neighbours share its edges; redraw the whole layer
}

bool PieLayer::OnCommand(ToolButtonID id)
{
	switch (id)
	{
		case TB_ShowSelOnlyOn:  if (ShowSelectedOnlyEnabled()) SetShowSelectedOnly( true); return true;
		case TB_ShowSelOnlyOff:                                SetShowSelectedOnly(false); return true;
	}
	return base_type::OnCommand(id);
}

void PieLayer::FillLcMenu(MenuData& menuData)
{
	// Not GraphicLayer::FillLcMenu: its "Classify ..." submenu would offer to classify the colour
	// attribute, which is a colour per element already, and a pie has no classified aspect.
	ScalableObject::FillLcMenu(menuData); // z-position: the order of the rings

	if (GetThemeDisplayItem())
		menuData.emplace_back(
			"Show Statistics of " + GetThemeDisplayName(this),
			std::make_unique<RequestClientCmd>(make_shared_tree(GetThemeDisplayItem(), existing_obj{}), CC_ShowStatistics),
			this
		);

	{
		SubMenu subMenu(menuData, SharedStr("Show chart as ..."));
		menuData.push_back(MenuItem(SharedStr("Pie"), make_MembFuncCmd(&PieLayer::SetDrawModePie), this
			, m_DrawMode == PieDrawMode::Pie ? MF_CHECKED : 0));
		menuData.push_back(MenuItem(SharedStr("Donut"), make_MembFuncCmd(&PieLayer::SetDrawModeDonut), this
			, m_DrawMode == PieDrawMode::Donut ? MF_CHECKED : 0));
	}

	menuData.AddSeparator();

	auto activeTheme = GetActiveTheme();
	SubMenu subMenu(menuData, SharedStr("Activate TreeItem of Layer Aspect")); // SUBMENU
	InsertThemeActivationMenu(menuData, activeTheme.get(), this);
	for (AspectNr aNr = AspectNr(0); aNr != AN_AspectCount; aNr = AspectNr(aNr + 1))
	{
		auto theme = GetTheme(aNr);
		if (theme != activeTheme)
			InsertThemeActivationMenu(menuData, theme.get(), this);
	}
}

//----------------------------------------------------------------------
// geometry
//----------------------------------------------------------------------

// fraction of the disc, clockwise from twelve o'clock -> angle in world coordinates (Y up)
static inline CrdType FractionToAngle(CrdType f)
{
	return 0.5 * PI - 2.0 * PI * f;
}

static inline CrdPoint PointAt(CrdType radius, CrdType angle)
{
	return shp2dms_order<CrdType>(radius * std::cos(angle), radius * std::sin(angle));
}

PieLayer::Ring PieLayer::GetRing() const
{
	// Several pie layers in one chart are concentric rings of equal width, the first layer
	// innermost. Computed at draw time from the live layer set, so adding or removing a layer
	// re-spaces every ring on the next repaint (as ChartLayer groups its bars).
	SizeT nrRings = 1, ringIndex = 0;
	if (auto ls = const_cast<PieLayer*>(this)->GetLayerSet().lock())
	{
		std::vector<const PieLayer*> pieLayers;
		for (gr_elem_index i = 0, ne = ls->NrEntries(); i != ne; ++i)
			if (auto* pl = dynamic_cast<const PieLayer*>(ls->GetEntry(i)))
				pieLayers.push_back(pl);
		if (!pieLayers.empty())
		{
			nrRings = pieLayers.size();
			for (SizeT k = 0; k != nrRings; ++k)
				if (pieLayers[k] == this) ringIndex = k;
		}
	}
	CrdType width = PIE_RADIUS / CrdType(nrRings);
	Ring ring{ ringIndex * width, (ringIndex + 1) * width };
	if (ringIndex == 0 && m_DrawMode == PieDrawMode::Donut)
		ring.inner = DONUT_HOLE * width;
	return ring;
}

bool PieLayer::SliceWorldPolygon(SizeT e, const Ring& ring, std::vector<CrdPoint>& poly) const
{
	poly.clear();
	if (e >= NrSlices() || IsEmptySlice(e))
		return false;

	CrdType f0 = m_CumFractions[e], f1 = m_CumFractions[e+1];
	UInt32 nrSeg = Max<UInt32>(1, UInt32(std::ceil((f1 - f0) / HIT_TEST_SEGMENT_FRACTION)));

	for (UInt32 i = 0; i <= nrSeg; ++i)
		poly.push_back(PointAt(ring.outer, FractionToAngle(f0 + (f1 - f0) * i / nrSeg)));
	if (ring.inner > 0)
		for (UInt32 i = nrSeg + 1; i-- != 0; )
			poly.push_back(PointAt(ring.inner, FractionToAngle(f0 + (f1 - f0) * i / nrSeg)));
	else
		poly.push_back(shp2dms_order<CrdType>(0.0, 0.0));
	return true;
}

SizeT PieLayer::FindSlice(CrdPoint worldPnt, const Ring& ring) const
{
	CrdType x = worldPnt.X(), y = worldPnt.Y();
	CrdType r = std::sqrt(x * x + y * y);
	if (r < ring.inner || r > ring.outer)
		return UNDEFINED_VALUE(SizeT);

	// the point's fraction of the disc, clockwise from twelve o'clock
	CrdType f = (0.5 * PI - std::atan2(y, x)) / (2.0 * PI);
	f -= std::floor(f);

	auto it = std::upper_bound(m_CumFractions.begin(), m_CumFractions.end(), f);
	if (it == m_CumFractions.begin())
		return UNDEFINED_VALUE(SizeT);
	SizeT e = (it - m_CumFractions.begin()) - 1;
	if (e >= NrSlices() || IsEmptySlice(e))
		return UNDEFINED_VALUE(SizeT);
	return e;
}

//----------------------------------------------------------------------
// update and draw
//----------------------------------------------------------------------

void PieLayer::DoUpdateView()
{
	m_Ready = false;

	auto featureTheme = GetTheme(AN_Feature);
	const AbstrDataItem* valueAttr = featureTheme ? featureTheme->GetPaletteAttr() : nullptr;
	if (!valueAttr)
		return;
	if (!PrepareDataOrUpdateViewLater(valueAttr))
		return;

	auto colorTheme = GetEnabledTheme(AN_BrushColor); // a colour per element of E
	const AbstrDataItem* colorAttr = colorTheme ? colorTheme->GetThemeAttr() : nullptr;
	if (colorAttr && !PrepareDataOrUpdateViewLater(colorAttr))
		return;

	auto selTheme = GetTheme(AN_Selections);
	const AbstrDataItem* selAttr = selTheme ? selTheme->GetThemeAttr() : nullptr;
	if (selAttr && !PrepareDataOrUpdateViewLater(selAttr))
		return;

	ThemeReadLocks readLocks;
	if (!readLocks.push_back(featureTheme.get(), DrlType::Suspendible))
	{
		readLocks.ProcessFailOrSuspend(this);
		return;
	}
	if (colorTheme && !readLocks.push_back(colorTheme.get(), DrlType::Suspendible))
	{
		readLocks.ProcessFailOrSuspend(this);
		return;
	}
	if (selTheme && !readLocks.push_back(selTheme.get(), DrlType::Suspendible))
	{
		readLocks.ProcessFailOrSuspend(this);
		return;
	}

	auto valueData = valueAttr->GetRefObj();
	SizeT n = valueData->GetTiledRangeData()->GetElemCount();

	WeakPtr<const AbstrThemeValueGetter> colorGetter;
	if (colorTheme)
		colorGetter = colorTheme->GetValueGetter();

	std::optional<DataArray<SelectionID>::locked_cseq_t> selValues;
	if (selAttr)
		selValues = const_array_cast<SelectionID>(selAttr)->GetLockedDataRead();

	std::vector<Float64> parts(n);
	m_Colors.assign(n, s_DefaultSliceColor);
	m_Selected.assign(n, false);

	// A null is not a part, and a negative value cannot be part of a total: neither is drawn, and
	// neither counts in the total. A boolean attribute counts every true as one.
	Float64 total = 0.0;
	SizeT nrNull = 0, nrNegative = 0;
	for (SizeT e = 0; e != n; ++e)
	{
		Float64 v = valueData->GetValueAsFloat64(e);
		if (!IsDefined(v))
			{ v = 0.0; ++nrNull; }
		else if (v < 0.0)
			{ v = 0.0; ++nrNegative; }
		parts[e] = v;
		total += v;

		if (colorGetter && e < colorGetter->GetCount())
			m_Colors[e] = colorGetter->GetColorValue(e);
		if (selValues && e < selValues->size() && Bool((*selValues)[e]))
			m_Selected[e] = true;
	}
	m_Total = total;

	m_CumFractions.resize(n + 1);
	m_CumFractions[0] = 0.0;
	Float64 running = 0.0;
	for (SizeT e = 0; e != n; ++e)
	{
		running += parts[e];
		m_CumFractions[e+1] = (total > 0.0) ? running / total : 0.0;
	}
	if (total > 0.0)
		m_CumFractions[n] = 1.0; // the last slice closes the disc exactly, whatever rounding did along the way

	if (nrNegative && !m_ReportedSkipped)
	{
		m_ReportedSkipped = true;
		reportF(SeverityTypeID::ST_Warning, "Pie Chart of {}: {} negative value(s) are not part of the total and are not drawn"
			, valueAttr->GetFullName().c_str()
			, nrNegative
		);
	}

	m_Ready = true;

	// every ring shares the disc's extent
	SetWorldClientRect(
		CrdRect(
			shp2dms_order<CrdType>(-PIE_WORLD_EXTENT, -PIE_WORLD_EXTENT),
			shp2dms_order<CrdType>( PIE_WORLD_EXTENT,  PIE_WORLD_EXTENT)
		)
	);

	if (!m_ZoomedOnce)
	{
		m_ZoomedOnce = true;
		auto dv = GetDataView().lock();
		auto vp = GetViewPort();
		auto ls = GetLayerSet().lock();
		// only an initial solitary layer adopts the view (see HistogramLayer): it must override the
		// degenerate fallback ROI that AL_ZoomAll set while data was still pending; a layer dropped
		// into an existing chart shares the disc anyway
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

// the chart's text font at a given size, as AxisControl selects it for its tick labels
static void SetChartFont(DrawContext* dc, FontSizeCategory fsc)
{
#ifdef _WIN32
	auto fontHeight = GetDefaultFontHeightDIP(fsc) * (96.0 / 72.0);
	dc->SetFont("Noto Sans Medium", fontHeight, 0);
#else
	auto fontHeight = GetDefaultFontHeightDIP(fsc) * (72.0 / 96.0);
	dc->SetFont("Noto Sans", fontHeight, 0);
#endif
}

// black on a light slice, white on a dark one
static DmsColor ContrastingTextColor(DmsColor fill)
{
	UInt32 luminance = (299u * GetRed(fill) + 587u * GetGreen(fill) + 114u * GetBlue(fill)) / 1000u;
	return (luminance < 128) ? CombineRGB(255, 255, 255) : CombineRGB(0, 0, 0);
}

bool PieLayer::Draw(GraphDrawer& d) const
{
	if (!d.DoDrawData() || !m_Ready)
		return false;

	auto* dc = d.GetDrawContext();
	auto w2d = d.GetTransformation();
	auto toDev = [&w2d](CrdPoint wp) -> GPoint { return CrdPoint2GPoint(w2d.Apply(wp)); };

	Ring ring = GetRing();
	bool showSelOnly = ShowSelectedOnly();
	DmsColor selColor = COLORREF2DmsColor(GetSelectedClr());

	// the disc's device radius, taken off the transformation so that any zoom level or fit is honoured
	GPoint devCenter = toDev(shp2dms_order<CrdType>(0.0, 0.0));
	GPoint devEast   = toDev(PointAt(ring.outer, 0.0));
	CrdType devDx = devEast.x - devCenter.x, devDy = devEast.y - devCenter.y;
	CrdType devRadius = std::sqrt(devDx * devDx + devDy * devDy);
	CrdType devCircumference = 2.0 * PI * devRadius;

	SizeT n = NrSlices();
	SizeT focusElem = GetFocusElemIndex();

	if (!(m_Total > 0.0))
	{
		// no parts at all: the empty disc, so that the chart says "nothing to show" rather than nothing
		std::vector<GPoint> circle;
		UInt32 nrSeg = Max<UInt32>(12, UInt32(devCircumference / ARC_PIXELS_PER_VERTEX));
		for (UInt32 i = 0; i <= nrSeg; ++i)
			circle.push_back(toDev(PointAt(ring.outer, FractionToAngle(CrdType(i) / nrSeg))));
		dc->DrawPolyline(circle.data(), int(circle.size()), s_SliceFrameColor, 1);
		return false;
	}

	std::vector<GPoint> poly;
	for (SizeT e = 0; e != n; ++e)
	{
		if (IsEmptySlice(e))
			continue;
		if (showSelOnly && !m_Selected[e])
			continue;

		CrdType f0 = m_CumFractions[e], f1 = m_CumFractions[e+1];
		CrdType devArc = (f1 - f0) * devCircumference;
		UInt32 nrSeg = Max<UInt32>(1, UInt32(std::ceil(devArc / ARC_PIXELS_PER_VERTEX)));

		// the slice as a fan (pie) or a ring segment (donut, outer rings): outer arc clockwise,
		// then back along the inner arc, or to the centre
		poly.clear();
		for (UInt32 i = 0; i <= nrSeg; ++i)
			poly.push_back(toDev(PointAt(ring.outer, FractionToAngle(f0 + (f1 - f0) * i / nrSeg))));
		if (ring.inner > 0)
			for (UInt32 i = nrSeg + 1; i-- != 0; )
				poly.push_back(toDev(PointAt(ring.inner, FractionToAngle(f0 + (f1 - f0) * i / nrSeg))));
		else
			poly.push_back(devCenter);

		dc->DrawPolygon(poly.data(), int(poly.size()), m_Selected[e] ? selColor : m_Colors[e], DmsHatchStyle::Solid);
		if (devArc >= MIN_FRAMED_ARC_PIXELS)
		{
			poly.push_back(poly.front());
			dc->DrawPolyline(poly.data(), int(poly.size()), s_SliceFrameColor, 1);
		}

		if (e == focusElem)
		{
			GPoint c = toDev(PointAt(0.5 * (ring.inner + ring.outer), FractionToAngle(0.5 * (f0 + f1))));
			dc->DrawFocusRect(GRect(c.x - 3, c.y - 3, c.x + 4, c.y + 4));
		}
	}

	// the share of each slice, inside it when it fits; the total in the hole of a donut.
	// Drawn after all slices, so that no outline runs through a label.
	SetChartFont(dc, FontSizeCategory::MEDIUM);
	dc->SetBkMode(true);

	CrdType labelRadius = ring.inner + 0.6 * (ring.outer - ring.inner);
	CrdType radialRoom  = (ring.outer - ring.inner) / ring.outer * devRadius;
	for (SizeT e = 0; e != n; ++e)
	{
		if (IsEmptySlice(e))
			continue;
		if (showSelOnly && !m_Selected[e])
			continue;

		CrdType f0 = m_CumFractions[e], f1 = m_CumFractions[e+1];
		SharedStr text = mySSPrintF("{:.1f}%", 100.0 * (f1 - f0));
		GPoint extent = dc->GetTextExtent(text.c_str(), text.ssize());

		// room along the arc at the label's radius, and across the ring
		CrdType arcRoom = (f1 - f0) * 2.0 * PI * (labelRadius / ring.outer) * devRadius;
		if (extent.x > 0.9 * arcRoom || extent.y > 0.9 * radialRoom)
			continue;

		GPoint c = toDev(PointAt(labelRadius, FractionToAngle(0.5 * (f0 + f1))));
		dc->TextOut(GPoint(c.x - extent.x / 2, c.y - extent.y / 2), text.c_str(), text.ssize()
			, ContrastingTextColor(m_Selected[e] ? selColor : m_Colors[e]));
	}

	if (ring.inner > 0 && ring.inner < ring.outer && !(showSelOnly))
	{
		// the innermost ring is the only one with a hole; an outer ring has a ring inside it
		bool innermost = true;
		if (auto ls = const_cast<PieLayer*>(this)->GetLayerSet().lock())
			for (gr_elem_index i = 0, ne = ls->NrEntries(); i != ne; ++i)
				if (auto* pl = dynamic_cast<const PieLayer*>(ls->GetEntry(i)))
					{ innermost = (pl == this); break; }
		if (innermost)
		{
			SetChartFont(dc, FontSizeCategory::LARGE);
			SharedStr text = AsString(m_Total, FormattingFlags::ThousandSeparator);
			GPoint extent = dc->GetTextExtent(text.c_str(), text.ssize());
			CrdType holeDiameter = 2.0 * ring.inner / ring.outer * devRadius;
			if (extent.x <= 0.9 * holeDiameter && extent.y <= 0.9 * holeDiameter)
				dc->TextOut(GPoint(devCenter.x - extent.x / 2, devCenter.y - extent.y / 2), text.c_str(), text.ssize(), GraphicObject::GetDefaultTextColor());
		}
	}
	return false;
}

CrdRect PieLayer::CalcSelectedFullWorldRect() const
{
	CrdRect rect;
	if (!m_Ready)
		return rect;
	Ring ring = GetRing();
	std::vector<CrdPoint> poly;
	for (SizeT e = 0, n = NrSlices(); e != n; ++e)
		if (m_Selected[e] && SliceWorldPolygon(e, ring, poly))
			for (const auto& p : poly)
				rect |= CrdRect(p, p);
	return rect;
}

//----------------------------------------------------------------------
// the legend
//----------------------------------------------------------------------

void PieLayer::AddLegendColumns(PaletteControl* legend)
{
	// The legend is a table over E, one row per slice, that PaletteControl fills with the colour
	// swatch and the label. A pie's legend also needs the value and its share, and the share is a
	// calculated desktop item as the Count column of a classified legend is, with the same
	// definition as the slices: a null is no part, a negative value no part of the total.
	const AbstrDataItem* valueAttr = GetValueAttr();
	if (!valueAttr)
		return;
	auto dv = GetDataView().lock(); if (!dv) return;

	auto valueColumn = make_shared_gr<DataItemColumn>(legend, valueAttr)();
	legend->InsertColumn(valueColumn.get());

	if (!m_ShareAttr)
	{
		TreeItem* container = CreateDesktopContainer(dv->GetDesktopContext(), GetUltimateSourceItem(valueAttr));
		auto shareUnit = Unit<Float64>::GetStaticClass()->CreateDefault();
		SharedMutableDataItem shareAttr = CreateDataItem(container, GetTokenID_mt("Percentage"), valueAttr->GetAbstrDomainUnit(), shareUnit);
		shareAttr->SetDescr(SharedStr("Percentage of the total, the sum of the positive values"));
		shareAttr->SetKeepDataState(true);
		shareAttr->DisableStorage(true);
		auto valueName = valueAttr->GetFullName();
		// one decimal, as the labels in the slices: a legend is read, not computed from
		shareAttr->SetExpr(mySSPrintF("float64(round(1000.0 * max_elem(float64({0}), 0.0) / sum(max_elem(float64({0}), 0.0)))) / 10.0", valueName.c_str()));
		m_ShareAttr = shareAttr.get();
	}
	auto shareColumn = make_shared_gr<DataItemColumn>(legend, m_ShareAttr.get_ptr())();
	legend->InsertColumn(shareColumn.get());
}

//----------------------------------------------------------------------
// selection support: picked slices -> shared entity-domain selection attribute
//----------------------------------------------------------------------

void PieLayer::WriteSelection(const std::vector<char>& picked, EventID eventID)
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

	if (any || IsCreateNewEvent(eventID)) // a create-new event that picked nothing still clears the selection
	{
		writeLock.Commit();
		invBlock.ProcessChange();
	}
	InvalidateDraw();
	BroadcastUpdateRequest();
}

template <typename Hit>
void PieLayer::SelectSlices(Hit&& hit, EventID eventID)
{
	if (!m_Ready)
		return;
	Ring ring = GetRing();
	std::vector<char> picked(NrSlices(), 0);
	std::vector<CrdPoint> poly;
	for (SizeT e = 0, n = NrSlices(); e != n; ++e)
		if (SliceWorldPolygon(e, ring, poly) && hit(poly))
			picked[e] = 1;
	WriteSelection(picked, eventID);
}

void PieLayer::SelectPoint(CrdPoint worldPnt, EventID eventID)
{
	if (!m_Ready)
		return;

	// the slice under the click: focus it, or select it, as a feature layer does with its feature
	SizeT e = FindSlice(worldPnt, GetRing());

	InvalidationBlock invBlock(this);
	auto selTheme = CreateSelectionsTheme();
	MG_CHECK(selTheme);

	DataWriteLock writeLock(
		const_cast<AbstrDataItem*>(selTheme->GetThemeAttr()),
		CompoundWriteType(eventID)
	);
	if (SelectFeatureIndex(writeLock.get(), e, eventID))
	{
		writeLock.Commit();
		invBlock.ProcessChange();
	}
}

void PieLayer::SelectRect(CrdRect worldRect, EventID eventID)
{
	SelectSlices([worldRect](const std::vector<CrdPoint>& poly) { return PolygonIntersectsRect(begin_ptr(poly), end_ptr(poly), worldRect); }, eventID);
}

void PieLayer::SelectCircle(CrdPoint worldPnt, CrdType worldRadius, EventID eventID, const JacobianMatrix<CrdType>* /*worldEllipse*/)
{
	SelectSlices([worldPnt, worldRadius](const std::vector<CrdPoint>& poly) { return PolygonIntersectsCircle(begin_ptr(poly), end_ptr(poly), worldPnt, worldRadius); }, eventID);
}

void PieLayer::SelectPolygon(const CrdPoint* first, const CrdPoint* last, EventID eventID)
{
	if (first == last)
		return;
	SelectSlices([first, last](const std::vector<CrdPoint>& poly) { return PolygonsIntersect(begin_ptr(poly), end_ptr(poly), first, last); }, eventID);
}

IMPL_DYNC_LAYERCLASS(PieLayer, ASE_Feature|ASE_BrushColor|ASE_LabelText|ASE_Selections, AN_Feature, 0)
