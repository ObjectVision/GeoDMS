// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__SHV_CHARTDATAVIEW_H)
#define __SHV_CHARTDATAVIEW_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "DataView.h"

class ChartControl;

//----------------------------------------------------------------------
// class  : ChartDataView
//----------------------------------------------------------------------
// DataView for charts (issue #75); user-facing name "Graph View" / "Chart".
// Accepts numeric attributes and shows them as a histogram of classified values;
// composition and interaction reuse the MapView building blocks via ChartControl.

class ChartDataView : public DataView
{
	typedef DataView base_type;

public:
	ChartDataView(TreeItem* viewContext, ShvSyncMode sm);

	std::shared_ptr<ChartControl> GetContents();
	std::shared_ptr<const ChartControl> GetContents() const;

	auto GetViewType() const -> ViewStyle override { return ViewStyle::tvsHistogram; }

protected:
	bool CanContain(const TreeItem* viewCandidate) const override;
	void AddLayer(const TreeItem*, bool isDropped) override;
	ExportInfo GetExportInfo() override;
	SharedStr GetCaption() const override;
	const TreeItem* GetCaptionItem() const override;

private:
	void AddHistogramLayer(const AbstrDataItem* adi, bool isDropped);
	// scatter/line/bar: synthesize feature geometry over the value's domain and reuse
	// the standard FeatureLayer path, so selection/focus/themes work verbatim.
	void AddSeriesLayer(const AbstrDataItem* adi, ChartKind kind, bool isDropped);
	void ActivateAndZoom(GraphicLayer* layer, bool isDropped);

	DECL_RTTI(, Class)
};

#endif // !defined(__SHV_CHARTDATAVIEW_H)
