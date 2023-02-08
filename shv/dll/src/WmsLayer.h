//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>

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

	bool ZoomIn();
	bool ZoomOut();

protected:
//	override virtuals of GraphicObject
	bool  Draw(GraphDrawer& d) const override;

	void Zoom1To1() override;

	//	override virtuals of GraphicLayer
	const AbstrUnit* GetGeoCrdUnit() const override { throwIllegalAbstract(MG_POS, this, "GetGeoCrdUnit"); }

	CrdRect CalcSelectedFullWorldRect() const override { throwIllegalAbstract(MG_POS, this, "CalcSelectedFullWorldRect"); }
	void _InvalidateFeature(SizeT featureIndex) override { throwIllegalAbstract(MG_POS, this, "_InvalidateFeature"); }

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

