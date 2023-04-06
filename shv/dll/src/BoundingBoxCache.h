#ifndef __SHV_BOUNDINGBOXCACHE_H
#define __SHV_BOUNDINGBOXCACHE_H

#include "LockLevels.h"

#include "AbstrBoundingBoxCache.h"
#include "ParallelTiles.h"
#include "FeatureLayer.h"

extern std::map<const AbstrDataObject*, const AbstrBoundingBoxCache*> g_BB_Register;
extern leveled_critical_section cs_BB;

//----------------------------------------------------------------------
// class  : BoundingBoxCache
//----------------------------------------------------------------------

template <typename RectArrayType, typename FeatArrayType>
typename RectArrayType::value_type MakeBlockBoundArray(RectArrayType& blockArray, const FeatArrayType& featArray, UInt32 blockSize)
{
	SizeT n = featArray.size();

	blockArray.clear();
	blockArray.reserve( (n + (blockSize-1))/blockSize );
	auto
		i = featArray.begin(),
		e = featArray.end();
	while (i != e)
	{
		auto blockEnd = (SizeT(e-i) > blockSize) ? i+blockSize : e;
		blockArray.push_back(RectArrayType::value_type(i, blockEnd, false, true));
		i = blockEnd;
	}
	return RectArrayType::value_type(blockArray.begin(), blockArray.end(), false, false);
}

template <typename F>
struct BoundingBoxCache : AbstrBoundingBoxCache
{
	typedef Point<F>                                            PointType;
	typedef Range<PointType>                                    RectType;
	typedef std::vector<RectType>                               RectArrayType;
	typedef typename sequence_traits<PointType>::container_type PolygonType;
	typedef DataArray<PolygonType>                              DataArrayType;

	struct BoxData {
		RectArrayType m_FeatBoundArray;
		RectArrayType m_BlockBoundArray;
		RectType      m_TotalBound;
	};
	const BoxData& GetBoxData(tile_id t) const
	{
		if (!IsDefined(t))
		{
			dms_assert(m_BoxData.size() == 1);
			t = 0;
		}
		dms_assert(t< m_BoxData.size());
		return m_BoxData[t];
	}
	BoundingBoxCache(const AbstrDataObject* featureData);
	const RectArrayType& GetBoundsArray    (tile_id t) const { return GetBoxData(t).m_FeatBoundArray ; }
	const RectArrayType& GetBlockBoundArray(tile_id t) const { return GetBoxData(t).m_BlockBoundArray; }

	DRect GetTileBounds(tile_id t) const override
	{
		return Convert<DRect>(GetBoxData(t).m_TotalBound);
	}

	DRect GetBounds(tile_id t, tile_offset featureID) const override
	{
		dms_assert(featureID < GetBoxData(t).m_FeatBoundArray.size());
		return Convert<DRect>(GetBoxData(t).m_FeatBoundArray[featureID]);
	}

	DRect GetBounds(SizeT featureID) const override
	{
		tile_loc tl = m_FeatureData->GetTiledLocation(featureID);
		return GetBounds(tl.first, tl.second);
	}

	DRect GetBlockBounds(tile_id t, tile_offset blockNr) const override
	{
		dms_assert(blockNr < GetBoxData(t).m_BlockBoundArray.size());
		return Convert<DRect>(GetBoxData(t).m_BlockBoundArray[blockNr]);
	}

	SizeT GetFeatureCount() const override
	{
		SizeT result = 0;
		for (auto b=m_BoxData.begin(), e=m_BoxData.end(); b!=e; ++b)
			result += b->m_FeatBoundArray.size();
		return result;
	}
	std::vector<BoxData> m_BoxData;
};


template <typename F>
BoundingBoxCache<F>::BoundingBoxCache(const AbstrDataObject* featureData)
	:	AbstrBoundingBoxCache(featureData)
{
	dms_assert(featureData);

	const DataArrayType* da = debug_valcast<const DataArray<PolygonType>*>(featureData);
	dms_assert(da);

	tile_id tn=featureData->GetTiledRangeData()->GetNrTiles();
	m_BoxData.resize(tn);

	leveled_critical_section resultAccess(item_level_type(0), ord_level_type::BoundingBoxCache2, "BoundingBoxCacheResult");
	parallel_tileloop(tn, [this, da, &resultAccess](tile_id tp)
	{
		auto data = da->GetLockedDataRead(tp);
		auto
			i = data.begin(),
			e = data.end();
		
		BoxData resultBoxes;
		RectArrayType& featBoundArray = resultBoxes.m_FeatBoundArray;
		featBoundArray.resize(e-i);
		typename RectArrayType::iterator ri = featBoundArray.begin();
		for (; i != e; ++ri, ++i)
		{
			*ri = RangeFromSequence(i->begin(), i->end());
		}
		resultBoxes.m_TotalBound = MakeBlockBoundArray(resultBoxes.m_BlockBoundArray, featBoundArray, c_BlockSize);

		leveled_critical_section::scoped_lock resultLock(resultAccess);
		m_BoxData[tp] = std::move(resultBoxes);
	});
}

//----------------------------------------------------------------------
// class  : GetBoundingBoxCache
//----------------------------------------------------------------------

template <typename ScalarType>
const BoundingBoxCache<ScalarType>*
GetBoundingBoxCache(SharedPtr<const AbstrBoundingBoxCache>& bbCacheSlot, WeakPtr<const AbstrDataItem> featureItem, bool mustPrepare)
{
	dms_assert(featureItem);
	leveled_critical_section::scoped_lock lock(cs_BB);
	DataReadLock readLock(featureItem);
	dms_assert(featureItem->GetDataRefLockCount() > 0);

	const AbstrDataObject* featureData = featureItem->GetCurrRefObj();
	auto& bbCache = g_BB_Register[featureData];
	if (!bbCache)
	{
		auto bbPtr = std::make_unique<BoundingBoxCache<ScalarType>> (featureData);
		bbPtr->Register(); // remove from global cache upon destruction
		bbCache = bbPtr.release();
	}
	bbCacheSlot = bbCache; // assign (shared) ownership to provided slot
	return debug_cast<const BoundingBoxCache<ScalarType>*>(bbCache);
}

template <typename ScalarType>
const BoundingBoxCache<ScalarType>*
GetBoundingBoxCache(const FeatureLayer* layer)
{
	dms_assert(IsMetaThread());
	return GetBoundingBoxCache<ScalarType>(layer->m_BoundingBoxCache, layer->GetFeatureAttr(), true);
}

#endif // __SHV_BOUNDINGBOXCACHE_H
