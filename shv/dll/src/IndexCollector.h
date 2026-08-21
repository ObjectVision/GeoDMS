// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if !defined(__SHV_INDEXCOLLECTOR_H)
#define __SHV_INDEXCOLLECTOR_H

#include "geom/Point.h"
#include "vt/iterrange.h"
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

//	entity_id  GetEntityIndex (feature_id featureIndex) const;
	feature_id GetFeatureIndex(entity_id  entityIndex ) const;

	const AbstrUnit* GetFeatureDomain() const;

	DataReadLock GetDataItemReadLock() const;
	auto GetDataRead(tile_id t) const->DataArray<entity_id>::locked_cseq_t;
	tile_id GetNrTiles() const;
	tile_loc GetTileDataLocation(SizeT index) const;

	bool HasExtKey() const { return m_ExtKeyAttr != nullptr; }
	bool HasGeoRel() const { return m_GeoRelAttr != nullptr; }
	auto GetGeoRel() const { return m_GeoRelAttr; }

	void Release();

private:
	IndexCollector(index_collector_key key);

	// cache identification and results
	SharedDataItem                              m_ExtKeyAttr, m_GeoRelAttr;
	SharedDcInterestPtr                         m_DC;
	SharedPtr<const AbstrTileRangeData>         m_TileData;
//	mutable DataArray<entity_id>::locked_cseq_t m_Array;
};

#endif // !defined(__SHV_INDEXCOLLECTOR_H)
