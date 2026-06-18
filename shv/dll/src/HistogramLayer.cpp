// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "HistogramLayer.h"

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
#include "Theme.h"
#include "ThemeReadLocks.h"
#include "ThemeValueGetter.h"
#include "ViewPort.h"

//----------------------------------------------------------------------
// class  : HistogramLayer
//----------------------------------------------------------------------

HistogramLayer::HistogramLayer(GraphicObject* owner)
	:	base_type(owner, GetStaticClass())
{}

const AbstrUnit* HistogramLayer::GetGeoCrdUnit() const
{
	auto vp = GetViewPort();
	MG_CHECK(vp);
	return vp->GetWorldCrdUnit(); // the synthetic chart-space unit
}

void HistogramLayer::Sync(TreeItem* viewContext, ShvSyncMode sm)
{
	base_type::Sync(viewContext, sm);
	if (sm == SM_Load)
		m_ZoomedOnce = true; // reconstructed from a saved desktop: keep the restored ROI, don't auto-zoom
}

bool HistogramLayer::OnCommand(ToolButtonID id)
{
	switch (id)
	{
		case TB_ShowSelOnlyOn:  if (ShowSelectedOnlyEnabled()) SetShowSelectedOnly( true); return true;
		case TB_ShowSelOnlyOff:                                SetShowSelectedOnly(false); return true;
	}
	return base_type::OnCommand(id);
}

CrdRect HistogramLayer::BarWorldRect(SizeT k, SizeT height) const
{
	assert(k+1 < m_BinBounds.size());
	return CrdRect(
		shp2dms_order<CrdType>(m_BinBounds[k  ], 0.0),
		shp2dms_order<CrdType>(m_BinBounds[k+1], CrdType(height))
	);
}

CrdRect HistogramLayer::CalcSelectedFullWorldRect() const
{
	CrdRect rect;
	if (m_BinsReady)
		for (SizeT k = 0, n = NrBins(); k != n; ++k)
			if (m_SelCounts[k])
				rect |= BarWorldRect(k);
	return rect;
}

void HistogramLayer::InvalidateFeature(SizeT featureIndex)
{
	// bars aggregate many entities; redraw the whole layer
	InvalidateDraw();
}

void HistogramLayer::DoInvalidate() const
{
	base_type::DoInvalidate();
	m_BinsReady = false;
	m_ValueGetter = nullptr;
	const_cast<HistogramLayer*>(this)->InvalidateDraw();
}

void HistogramLayer::DoUpdateView()
{
	m_BinsReady = false;

	auto theme = GetActiveTheme();
	if (!theme)
		return;
	const AbstrDataItem* themeAttr = theme->GetThemeAttr();
	if (!themeAttr)
		return;

	if (!PrepareDataOrUpdateViewLater(themeAttr))
		return;
	const AbstrDataItem* breaks = theme->GetClassification();
	if (breaks && !PrepareDataOrUpdateViewLater(breaks))
		return;
	const AbstrDataItem* palette = theme->GetPaletteAttr();
	if (palette && !PrepareDataOrUpdateViewLater(palette))
		return;
	if (!breaks && !palette)
		return; // direct aspect theme: no class set to aggregate over

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

	auto vg = theme->GetValueGetter();
	if (!vg)
		return;

	// derive the bin count from read-locked data, not from possibly still write-locked units
	SizeT nrBins;
	if (breaks)
		nrBins = breaks->GetRefObj()->GetTiledRangeData()->GetElemCount();
	else
	{
		if (!palette)
			return;
		nrBins = palette->GetRefObj()->GetTiledRangeData()->GetElemCount();
	}
	if (!nrBins)
		return;

	m_BinBounds.resize(nrBins+1);
	if (breaks)
	{
		auto breakData = breaks->GetRefObj();
		for (SizeT k = 0; k != nrBins; ++k)
			m_BinBounds[k] = breakData->GetValueAsFloat64(k);
	}
	else
	{
		// unique-values palette: class id space on the X axis
		for (SizeT k = 0; k != nrBins; ++k)
			m_BinBounds[k] = CrdType(k);
		m_BinBounds[nrBins] = CrdType(nrBins);
	}

	m_Counts   .assign(nrBins, 0);
	m_SelCounts.assign(nrBins, 0);

	std::optional<DataArray<SelectionID>::locked_cseq_t> selValues;
	if (selAttr)
		selValues = const_array_cast<SelectionID>(selAttr)->GetLockedDataRead();

	auto attrData = themeAttr->GetRefObj();
	SizeT n = Min<SizeT>(attrData->GetTiledRangeData()->GetElemCount(), vg->GetCount());
	Float64 maxValue = MinValue<Float64>();
	for (SizeT e = 0; e != n; ++e)
	{
		entity_id c = vg->GetClassIndex(e);
		if (c >= nrBins)
			continue;
		++m_Counts[c];
		if (selValues && e < selValues->size() && Bool((*selValues)[e]))
			++m_SelCounts[c];
		if (breaks)
		{
			Float64 v = attrData->GetValueAsFloat64(e);
			if (IsDefined(v))
				MakeMax(maxValue, v);
		}
	}

	if (breaks)
	{
		// close the last bin at the data maximum; degenerate ranges get a unit-width tail
		Float64 lastBound = m_BinBounds[nrBins-1];
		m_BinBounds[nrBins] = (maxValue > lastBound) ? maxValue : lastBound + 1.0;
	}

	m_Colors.assign(nrBins, CombineRGB(0, 128, 255));
	if (auto paletteGetter = vg->CreatePaletteGetter())
		for (SizeT k = 0; k != nrBins; ++k)
			m_Colors[k] = paletteGetter->GetColorValue(k);

	m_ValueGetter = vg;
	m_BinsReady = true;

	SizeT maxCount = 1;
	for (SizeT k = 0; k != nrBins; ++k)
		MakeMax(maxCount, m_Counts[k]);

	// anchor the chart origin at (0, 0): counts are never negative and the value axis
	// only extends below zero when the thematic values do
	SetWorldClientRect(
		CrdRect(
			shp2dms_order<CrdType>(Min<CrdType>(0.0, m_BinBounds.front()), 0.0),
			shp2dms_order<CrdType>(m_BinBounds.back(), CrdType(maxCount))
		)
	);

	if (!m_ZoomedOnce)
	{
		m_ZoomedOnce = true;
		auto dv = GetDataView().lock();
		auto vp = GetViewPort();
		auto ls = GetLayerSet().lock();
		// only an initial solitary layer adopts the view: it must override the degenerate fallback
		// ROI that AL_ZoomAll set while data was still pending; a layer dropped into an existing
		// chart must not disturb the current view
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

bool HistogramLayer::Draw(GraphDrawer& d) const
{
	if (!d.DoDrawData())
		return false;
	if (!m_BinsReady)
		return false;

	auto* dc = d.GetDrawContext();
	auto w2d = d.GetTransformation();

	bool showSelOnly = ShowSelectedOnly();
	DmsColor selColor   = COLORREF2DmsColor(GetSelectedClr());
	DmsColor frameColor = CombineRGB(64, 64, 64);

	SizeT focusBin = UNDEFINED_VALUE(SizeT);
	SizeT focusElem = GetFocusElemIndex();
	if (IsDefined(focusElem) && m_ValueGetter && focusElem < m_ValueGetter->GetCount())
		focusBin = m_ValueGetter->GetClassIndex(focusElem);

	auto barDeviceRect = [&w2d](CrdRect barRect) -> GRect
	{
		w2d.InplApply(barRect);
		return CrdRect2GRect(barRect);
	};

	for (SizeT k = 0, nrBins = NrBins(); k != nrBins; ++k)
	{
		if (!m_Counts[k])
			continue;

		GRect barRect = barDeviceRect(BarWorldRect(k));
		if (!showSelOnly)
		{
			dc->FillRect(barRect, m_Colors[k]);

			GPoint outline[5] = {
				GPoint(barRect.left,    barRect.top),
				GPoint(barRect.right-1, barRect.top),
				GPoint(barRect.right-1, barRect.bottom-1),
				GPoint(barRect.left,    barRect.bottom-1),
				GPoint(barRect.left,    barRect.top)
			};
			dc->DrawPolyline(outline, 5, frameColor, 1);
		}
		if (m_SelCounts[k])
			dc->FillRect(barDeviceRect(BarWorldRect(k, m_SelCounts[k])), selColor);

		if (k == focusBin)
			dc->DrawFocusRect(barRect);
	}
	return false;
}

//----------------------------------------------------------------------
// selection support: picked bins -> shared entity-domain selection attribute
//----------------------------------------------------------------------

void HistogramLayer::SelectBins(const std::vector<bool>& pickedBins, EventID eventID)
{
	if (!m_BinsReady)
		return;
	auto vg = m_ValueGetter;
	if (!vg)
		return;

	auto selTheme = CreateSelectionsTheme();
	MG_CHECK(selTheme);

	InvalidationBlock invBlock(this);
	DataWriteLock writeLock(
		const_cast<AbstrDataItem*>(selTheme->GetThemeAttr()),
		CompoundWriteType(eventID)
	);

	bool isAdd = !(eventID & EventID::CTRLKEY);
	auto selData = mutable_array_cast<SelectionID>(writeLock.get())->GetDataWrite(no_tile, dms_rw_mode::read_write);

	SizeT n = Min<SizeT>(selData.size(), vg->GetCount());
	SizeT nrBins = NrBins();
	bool anyPicked = false;
	for (SizeT e = 0; e != n; ++e)
	{
		entity_id c = vg->GetClassIndex(e);
		if (c < nrBins && pickedBins[c])
		{
			selData[e] = isAdd;
			anyPicked = true;
		}
	}

	if (anyPicked || IsCreateNewEvent(eventID)) // a create-new event that picked nothing still clears the selection
	{
		writeLock.Commit();
		invBlock.ProcessChange();
	}
	InvalidateDraw();
	BroadcastUpdateRequest();
}

void HistogramLayer::SelectPoint(CrdPoint worldPnt, EventID eventID)
{
	if (!m_BinsReady)
		return;
	if (!(eventID & EventID::REQUEST_SEL))
		return; // a bar aggregates many entities: no single focus element to assign (MVP)

	SizeT nrBins = NrBins();
	std::vector<bool> picked(nrBins, false);
	for (SizeT k = 0; k != nrBins; ++k)
		if (m_Counts[k] && IsIncluding(BarWorldRect(k), worldPnt))
			picked[k] = true;
	SelectBins(picked, eventID);
}

void HistogramLayer::SelectRect(CrdRect worldRect, EventID eventID)
{
	if (!m_BinsReady)
		return;
	SizeT nrBins = NrBins();
	std::vector<bool> picked(nrBins, false);
	for (SizeT k = 0; k != nrBins; ++k)
		if (m_Counts[k] && IsIntersecting(worldRect, BarWorldRect(k)))
			picked[k] = true;
	SelectBins(picked, eventID);
}

void HistogramLayer::SelectCircle(CrdPoint worldPnt, CrdType worldRadius, EventID eventID, const JacobianMatrix<CrdType>* /*worldEllipse*/)
{
	// bars are world-axis-aligned rects; clamp the center into the bar to find the nearest bar point
	if (!m_BinsReady)
		return;
	SizeT nrBins = NrBins();
	std::vector<bool> picked(nrBins, false);
	for (SizeT k = 0; k != nrBins; ++k)
	{
		if (!m_Counts[k])
			continue;
		CrdRect bar = BarWorldRect(k);
		CrdPoint nearest(
			Min<CrdType>(Max<CrdType>(worldPnt.first,  bar.first.first ), bar.second.first ),
			Min<CrdType>(Max<CrdType>(worldPnt.second, bar.first.second), bar.second.second)
		);
		if (SqrDist<CrdType>(nearest, worldPnt) <= worldRadius * worldRadius)
			picked[k] = true;
	}
	SelectBins(picked, eventID);
}

static bool SegmentsCross(CrdPoint a, CrdPoint b, CrdPoint c, CrdPoint d)
{
	auto orient = [](CrdPoint p, CrdPoint q, CrdPoint r) -> int
	{
		CrdType v = (q.first - p.first) * (r.second - p.second)
		          - (q.second - p.second) * (r.first - p.first);
		return (v > 0) - (v < 0);
	};
	return orient(a, b, c) * orient(a, b, d) < 0
	    && orient(c, d, a) * orient(c, d, b) < 0;
}

static bool PolygonIntersectsRect(const CrdPoint* first, const CrdPoint* last, const CrdRect& rect)
{
	// any polygon vertex inside the rect?
	for (const CrdPoint* i = first; i != last; ++i)
		if (IsIncluding(rect, *i))
			return true;

	// any rect corner inside the polygon?
	CrdPoint corners[4] = {
		rect.first,
		CrdPoint(rect.first.first,  rect.second.second),
		rect.second,
		CrdPoint(rect.second.first, rect.first.second)
	};
	for (auto corner : corners)
		if (IsInside(first, last, corner))
			return true;

	// any polygon edge crossing a rect edge?
	for (const CrdPoint* i = first; i != last; ++i)
	{
		const CrdPoint* j = i + 1; if (j == last) j = first;
		for (UInt32 c = 0; c != 4; ++c)
			if (SegmentsCross(*i, *j, corners[c], corners[(c+1) % 4]))
				return true;
	}
	return false;
}

void HistogramLayer::SelectPolygon(const CrdPoint* first, const CrdPoint* last, EventID eventID)
{
	if (!m_BinsReady || first == last)
		return;
	SizeT nrBins = NrBins();
	std::vector<bool> picked(nrBins, false);
	for (SizeT k = 0; k != nrBins; ++k)
		if (m_Counts[k] && PolygonIntersectsRect(first, last, BarWorldRect(k)))
			picked[k] = true;
	SelectBins(picked, eventID);
}

IMPL_DYNC_LAYERCLASS(HistogramLayer, ASE_BrushColor|ASE_Selections, AN_BrushColor, 1)
