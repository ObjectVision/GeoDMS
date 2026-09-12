// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __SHV_PIELAYER_H
#define __SHV_PIELAYER_H

#include "GraphicLayer.h"

#include "vt/color.h"
#include "ptr/InterestHolders.h"

//----------------------------------------------------------------------
// class  : PieLayer
//----------------------------------------------------------------------
// The parts of a total (issue #1273): one slice per element of the entity domain E of a
// numeric attribute, sized by that element's share in the attribute's sum, drawn in
// chart-space world coordinates as a disc of radius 1 around the origin. A draw mode
// turns the disc into a ring (donut) that carries the total in its hole.
//
// The value attribute rides the AN_Feature theme, as the geometry of a feature layer
// does: a slice's shape IS its value. That makes the value attribute the layer's active
// attribute, so the caption, the legend header, the statistics entry and the entity
// domain of the shared AN_Selections attribute follow from it. Colours come from an
// AN_BrushColor theme over E itself, one colour per element, not from a classification
// of the values: in a pie every part has to be told from its neighbours.
//
// Like HistogramLayer the drawing domain is E, so selection writes the shared
// AN_Selections attribute and stays in sync with Map- and TableViews.

enum class PieDrawMode : UInt8 { Pie, Donut };

class PaletteControl;

class PieLayer : public GraphicLayer
{
	typedef GraphicLayer base_type;

public:
	PieLayer(GraphicObject* owner);

	void SetDrawMode(PieDrawMode m);
	PieDrawMode GetDrawMode() const { return m_DrawMode; }

	const AbstrDataItem* GetValueAttr() const; // the AN_Feature theme's attribute: E -> V

//	override virtuals of GraphicLayer
	const AbstrUnit* GetGeoCrdUnit() const override;
	CrdRect CalcSelectedFullWorldRect() const override;
	void InvalidateFeature(SizeT featureIndex) override;
	void AddLegendColumns(PaletteControl* legend) override;

	void SelectPoint  (CrdPoint worldPnt, EventID eventID) override;
	void SelectRect   (CrdRect worldRect, EventID eventID) override;
	void SelectCircle (CrdPoint worldPnt, CrdType worldRadius, EventID eventID, const JacobianMatrix<CrdType>* worldEllipse = nullptr) override;
	void SelectPolygon(const CrdPoint* first, const CrdPoint* last, EventID eventID) override;

//	override virtuals of GraphicObject
	void Sync(TreeItem* viewContext, ShvSyncMode sm) override; // persist draw mode; suppress auto-zoom on reload
	bool OnCommand(ToolButtonID id) override;
	void FillLcMenu(MenuData& menuData) override;
	bool ShowSelectedOnlyEnabled() const override { return true; }
	void UpdateShowSelOnly() override { InvalidateDraw(); }

protected:
	void DoUpdateView() override;
	bool Draw(GraphDrawer& d) const override;

//	override virtuals of Actor
	void DoInvalidate() const override;

private:
	// The radii of this layer's ring. Several pie layers in one chart become concentric rings,
	// the first layer innermost; only the innermost ring can have the donut hole.
	struct Ring { CrdType inner, outer; };
	Ring GetRing() const;

	SizeT NrSlices() const { return m_CumFractions.empty() ? 0 : m_CumFractions.size() - 1; }
	bool  IsEmptySlice(SizeT e) const { return !(m_CumFractions[e+1] > m_CumFractions[e]); }
	// the slice as a world polygon (arc sampled), false for an empty slice
	bool  SliceWorldPolygon(SizeT e, const Ring& ring, std::vector<CrdPoint>& poly) const;
	// the slice under a world point, or UNDEFINED_VALUE(SizeT)
	SizeT FindSlice(CrdPoint worldPnt, const Ring& ring) const;

	// select every slice whose world polygon satisfies `hit`
	template <typename Hit> void SelectSlices(Hit&& hit, EventID eventID);
	// per-element selection: write the shared AN_Selections for the picked element mask.
	void WriteSelection(const std::vector<char>& picked, EventID eventID);

	void SetDrawModePie()   { SetDrawMode(PieDrawMode::Pie); }
	void SetDrawModeDonut() { SetDrawMode(PieDrawMode::Donut); }

	PieDrawMode m_DrawMode = PieDrawMode::Pie;

	// per-element state, (re)computed by DoUpdateView: slice e covers the fraction
	// [m_CumFractions[e], m_CumFractions[e+1]] of the disc, clockwise from twelve o'clock
	mutable std::vector<CrdType>  m_CumFractions; // size NrSlices()+1
	mutable std::vector<DmsColor> m_Colors;
	mutable std::vector<bool>     m_Selected;
	mutable Float64 m_Total = 0.0;     // the sum of the positive values
	mutable bool m_Ready = false;
	bool m_ZoomedOnce = false;
	bool m_ReportedSkipped = false;    // the log line about skipped values goes out once

	SharedDataItemInterestPtr m_ShareAttr; // the legend's percentage column, a calculated desktop item

	DECL_RTTI(, LayerClass)
};

#endif // __SHV_PIELAYER_H
