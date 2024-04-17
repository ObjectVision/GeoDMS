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

#if !defined(__SHV_INDEXCOLLECTOR_H)
#define __SHV_INDEXCOLLECTOR_H

#include "geo/Point.h"
#include "geo/iterrange.h"
#include "ptr/OwningPtr.h"
#include "ptr/InterestHolders.h"
#include "ptr/SharedPtr.h"

#include "DataArray.h"
#include "DataLocks.h"
#include "DataController.h"
#include "ShvBase.h"

using SharedDcInterestPtr = InterestPtr<DataControllerRef>;

//----------------------------------------------------------------------
// struct  : IndexCollector
//----------------------------------------------------------------------

typedef Point<const AbstrDataItem* > index_collector_key;
index_collector_key GetIndexCollectorKey(const Theme* featureTheme);

struct IndexCollector : public SharedBase
{
 	static SharedPtr<IndexCollector> Create(index_collector_key);
	static SharedPtr<IndexCollector> Create(const Theme* featureTheme);
	~IndexCollector();

	entity_id  GetEntityIndex (feature_id featureIndex) const;
	feature_id GetFeatureIndex(entity_id  entityIndex ) const;

	const AbstrUnit* GetFeatureDomain() const;

	DataReadLock GetDataItemReadLock(tile_id t) const;
	tile_id GetNrTiles() const;
	tile_loc GetTiledLocation(SizeT index) const;

	bool HasExtKey() const { return m_ExtKeyAttr; }
	bool HasGeoRel() const { return m_GeoRelAttr; }

	void Release();

private:
	IndexCollector(index_collector_key key);

	// cache identification and results
	SharedDataItem                              m_ExtKeyAttr, m_GeoRelAttr;
	SharedDcInterestPtr                         m_DC;
	SharedPtr<const AbstrTileRangeData>         m_TileData;
	mutable DataArray<entity_id>::locked_cseq_t m_Array;
};

#endif // !defined(__SHV_INDEXCOLLECTOR_H)
