// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __SHV_HISTOGRAMLAYER_H
#define __SHV_HISTOGRAMLAYER_H

#include "GraphicLayer.h"

#include "geo/color.h"

class AbstrThemeValueGetter;

//----------------------------------------------------------------------
// class  : HistogramLayer
//----------------------------------------------------------------------
// Bars of element counts per class of the active (classified) theme, drawn in
// chart-space world coordinates: X = thematic value (class-break bounds), Y = count.
// Unlike FeatureLayers, the drawing domain is the class set; selection operations
// translate picked bins back to the entity domain E by writing the shared
// AN_Selections attribute, so selections stay in sync with Map- and TableViews.

class HistogramLayer : public GraphicLayer
{
	typedef GraphicLayer base_type;

public:
	HistogramLayer(GraphicObject* owner);

//	override virtuals of GraphicLayer
	const AbstrUnit* GetGeoCrdUnit() const override;
	CrdRect CalcSelectedFullWorldRect() const override;
	void InvalidateFeature(SizeT featureIndex) override;

	void SelectPoint  (CrdPoint worldPnt, EventID eventID) override;
	void SelectRect   (CrdRect worldRect, EventID eventID) override;
	void SelectCircle (CrdPoint worldPnt, CrdType worldRadius, EventID eventID) override;
	void SelectPolygon(const CrdPoint* first, const CrdPoint* last, EventID eventID) override;

//	override virtuals of GraphicObject
	void Sync(TreeItem* viewContext, ShvSyncMode sm) override; // suppress auto-zoom on reload (respect restored ROI)
	bool OnCommand(ToolButtonID id) override;
	bool ShowSelectedOnlyEnabled() const override { return true; }
	void UpdateShowSelOnly() override { InvalidateDraw(); }

protected:
	void DoUpdateView() override;
	bool Draw(GraphDrawer& d) const override;

//	override virtuals of Actor
	void DoInvalidate() const override;

private:
	SizeT NrBins() const { return m_Counts.size(); }
	CrdRect BarWorldRect(SizeT k) const { return BarWorldRect(k, m_Counts[k]); }
	CrdRect BarWorldRect(SizeT k, SizeT height) const;
	void SelectBins(const std::vector<bool>& pickedBins, EventID eventID);

	// bin state, (re)computed by DoUpdateView from the active theme + shared selections
	mutable std::vector<Float64>  m_BinBounds; // ascending, size NrBins()+1
	mutable std::vector<SizeT>    m_Counts, m_SelCounts;
	mutable std::vector<DmsColor> m_Colors;
	mutable WeakPtr<const AbstrThemeValueGetter> m_ValueGetter; // owned by the active Theme; reset on DoInvalidate
	mutable bool m_BinsReady = false;
	bool m_ZoomedOnce = false;

	DECL_RTTI(SHV_CALL, LayerClass)
};

#endif // __SHV_HISTOGRAMLAYER_H
