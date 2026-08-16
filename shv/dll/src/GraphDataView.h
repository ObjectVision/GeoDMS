// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__SHV_GRAPHDATAVIEW_H)
#define __SHV_GRAPHDATAVIEW_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "DataView.h"

//----------------------------------------------------------------------
// class  : GraphDataView
//----------------------------------------------------------------------

class GraphDataView : public DataView
{
	typedef DataView base_type;

public:
	GraphDataView(TreeItem* viewContext, ShvSyncMode sm);

	std::shared_ptr < MapControl> GetContents();
	std::shared_ptr < const MapControl> GetContents() const;

	auto GetViewType() const -> ViewStyle override { return ViewStyle::tvsMapView; }

protected:
	bool CanContain(const TreeItem* viewCandidate) const override;
	void AddLayer(const TreeItem*, bool isDropped) override;
	ExportInfo GetExportInfo() override;
	SharedStr GetCaption() const override;
	const TreeItem* GetCaptionItem() const override; // #418: active-layer entity, as a disambiguation peer

private:
	LayerInfo GetCompleteLayerInfoOrThrow(const TreeItem* viewItem) const;

	DECL_RTTI(, Class)
};

#endif // !defined(__SHV_GRAPHDATAVIEW_H)
