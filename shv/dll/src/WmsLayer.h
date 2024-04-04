// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __SHV_WMSLAYER_H
#define __SHV_WMSLAYER_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "GridLayerBase.h"

namespace wms {
	typedef UPoint tile_pos;
	typedef UPoint raster_pos;
	typedef std::pair<UInt32, tile_pos> tile_id;

	enum class image_format_type {
		png,
		jpeg,
		other
	};

	struct tile_matrix {
		SharedStr m_Name;
		CrdTransformation m_Raster2World;
		WPoint    m_TileSize;
		tile_pos  m_MatrixSize;

		raster_pos RasterSize() const;
		raster_pos RasterPosition(tile_pos t) const;
		Range<raster_pos> RasterExtents(tile_pos t) const;
		CrdRect WorldExtents() const;
		CrdPoint WorldPosition(tile_pos t) const;
		CrdRect WorldExtents(tile_pos t) const;
		grid_coord_key GridCoordKey() const;
	};

	typedef std::vector<tile_matrix> tile_matrix_set;
	struct TileLoader;
	struct TileCache;
}	// namespace wms

//----------------------------------------------------------------------
// class  : GraphicGridLayer
//----------------------------------------------------------------------

struct WmsLayer : GridLayerBase
{
	typedef GridLayerBase base_type;
	WmsLayer(GraphicObject* owner);
	~WmsLayer();

	void SetSpecContainer(const TreeItem* specContainer);
	void SetWorldCrdUnit(const AbstrUnit* WorldCrdUnit);

	bool ZoomIn(ViewPort* vp);
	bool ZoomOut(ViewPort* vp, bool justClickIsOK);

protected:
//	override virtuals of GraphicObject
	bool Draw(GraphDrawer& d) const override;

	void Zoom1To1(ViewPort* vp) override;

	//	override virtuals of GraphicLayer
	const AbstrUnit* GetGeoCrdUnit() const override { throwIllegalAbstract(MG_POS, this, "GetGeoCrdUnit"); }

	CrdRect CalcSelectedFullWorldRect() const override { throwIllegalAbstract(MG_POS, this, "CalcSelectedFullWorldRect"); }
	void InvalidateFeature(SizeT featureIndex) override { throwIllegalAbstract(MG_POS, this, "_InvalidateFeature"); }

	void Sync(TreeItem* viewContext, ShvSyncMode sm);

private: 
	friend wms::TileLoader; 
	friend wms::TileCache;

	void RunTileLoads(bool maySuspend) const;

	SharedPtr<const TreeItem> m_SpecContainer;
	wms::tile_matrix_set m_TMS;
	std::unique_ptr<wms::TileCache> m_TileCache;
	mutable SizeT m_ZoomLevel = 0;
	const AbstrUnit* m_WorldCrdUnit = nullptr;

	CrdRect WorldExtents(wms::tile_id key) const {
		const wms::tile_matrix& tm = m_TMS[key.first];
		return tm.WorldExtents(key.second);
	}
	DECL_RTTI(SHV_CALL, LayerClass);
};


#endif // __SHV_WMSLAYER_H

