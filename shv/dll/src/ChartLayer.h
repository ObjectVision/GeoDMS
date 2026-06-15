// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __SHV_CHARTLAYER_H
#define __SHV_CHARTLAYER_H

#include "GraphicLayer.h"

#include "geo/color.h"
#include "ptr/InterestHolders.h"

class AbstrThemeValueGetter;

//----------------------------------------------------------------------
// class  : ChartLayer
//----------------------------------------------------------------------
// A per-element series layer (issue #75) over an entity domain E, drawn in chart-space
// world coordinates. It takes the X and Y axes as separate attributes:
//   - Y = the active theme's thematic attribute (the value shown on the vertical axis);
//   - X = a settable attribute over E (horizontal axis); when null, the element's row
//         number (id of E) is used.
// A draw mode selects markers (scatter), a connecting polyline (line), or both, so
// switching between scatter and line is a cheap toggle rather than a different layer.
// Like HistogramLayer the drawing domain is E, so selection writes the shared
// AN_Selections attribute and stays in sync with Map- and TableViews.

enum class ChartDrawMode : UInt8 { Points, Line, PointsAndLine, Bars };

class ChartLayer : public GraphicLayer
{
	typedef GraphicLayer base_type;

public:
	ChartLayer(GraphicObject* owner);

	void SetDrawMode(ChartDrawMode m);
	ChartDrawMode GetDrawMode() const { return m_DrawMode; }

	// X-axis management: set the attribute driving the horizontal axis (must share the Y
	// attribute's domain E); pass nullptr to use the row number (id of E).
	void SetXAttr(const AbstrDataItem* xAttr);
	const AbstrDataItem* GetXAttr() const { return m_XAttr.get_ptr(); }

	// categorical X tick labels (position -> label) for the horizontal AxisControl; empty for a
	// numeric X axis. Queried by AxisControl on the chart's active layer.
	const std::vector<std::pair<CrdType, SharedStr>>& GetXAxisLabels() const { return m_XAxisLabels; }

//	override virtuals of GraphicLayer
	const AbstrUnit* GetGeoCrdUnit() const override;
	CrdRect CalcSelectedFullWorldRect() const override;
	void InvalidateFeature(SizeT featureIndex) override;

	void SelectPoint  (CrdPoint worldPnt, EventID eventID) override;
	void SelectRect   (CrdRect worldRect, EventID eventID) override;
	void SelectCircle (CrdPoint worldPnt, CrdType worldRadius, EventID eventID) override;
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
	ActorVisitState VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const override;
	void DoInvalidate() const override;

private:
	// per-element selection: write the shared AN_Selections for the picked element mask.
	void WriteSelection(const std::vector<char>& picked, EventID eventID);
	// select every element e whose world point satisfies `hit`.
	template <typename Hit> void SelectElems(Hit&& hit, EventID eventID);
	void SetDrawModePoints()        { SetDrawMode(ChartDrawMode::Points); }
	void SetDrawModeLine()          { SetDrawMode(ChartDrawMode::Line); }
	void SetDrawModePointsAndLine() { SetDrawMode(ChartDrawMode::PointsAndLine); }
	void SetDrawModeBars()          { SetDrawMode(ChartDrawMode::Bars); }

	// X-axis picker: enumerate numeric attributes over Y's domain E as candidate X axes.
	void AddXAxisMenu(MenuData& menuData);

	SharedDataItemInterestPtr m_XAttr; // optional; null => row number
	ChartDrawMode m_DrawMode = ChartDrawMode::Points;

	// per-element state, (re)computed by DoUpdateView in world coords (X=col, Y=row)
	mutable std::vector<CrdPoint> m_Points;
	mutable std::vector<DmsColor> m_PointColors;
	mutable std::vector<bool>     m_Selected;
	mutable CrdType               m_BarSlotHalf = 0.4; // world half-width of a per-element bar slot (shared by sibling bar layers)
	// categorical X: tick labels at ordinal positions (empty => numeric X axis)
	mutable std::vector<std::pair<CrdType, SharedStr>> m_XAxisLabels;
	mutable bool m_Ready = false;
	bool m_ZoomedOnce = false;
	bool m_ZoomPending = false; // request a one-shot zoom-to-fit (e.g. after the X attribute changed)

	DECL_RTTI(SHV_CALL, LayerClass)
};

#endif // __SHV_CHARTLAYER_H
