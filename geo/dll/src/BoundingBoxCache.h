#ifndef __GEO_BOUNDINGBOXCACHE_H
#define __GEO_BOUNDINGBOXCACHE_H

#include "LockLevels.h"

#include "AbstrBoundingBoxCache.h"
#include "ParallelTiles.h"

extern std::map<const AbstrDataObject*, const AbstrBoundingBoxCache*> g_BB_Register;
extern leveled_critical_section cs_BB;

//----------------------------------------------------------------------
// class  : BoundingBoxCache
//----------------------------------------------------------------------

template <typename RectArrayType, typename FeatArrayType>
typename RectArrayType::value_type MakeBlockBoundArray(RectArrayType& blockArray, const FeatArrayType& featArray)
{
	SizeT n = featArray.size();

	blockArray.clear();
	blockArray.reserve( (n + (AbstrBoundingBoxCache::c_BlockSize-1))/ AbstrBoundingBoxCache::c_BlockSize);
	auto
		i = featArray.begin(),
		e = featArray.end();

	SizeT nrFullBlocks = n / AbstrBoundingBoxCache::c_BlockSize;
	while (nrFullBlocks--)
	{
		auto blockEnd = i + AbstrBoundingBoxCache::c_BlockSize;
		blockArray.push_back(RectArrayType::value_type(i, blockEnd, false, true));
		i = blockEnd;
	}
	if (i!=e)
		blockArray.push_back(RectArrayType::value_type(i, e, false, true));
	assert(blockArray.size() == blockArray.capacity());
	return RectArrayType::value_type(blockArray.begin(), blockArray.end(), false, false);
}

template <typename F>
struct PointBoundingBoxCache : AbstrBoundingBoxCache
{
	using PointType = Point<F>;
	using RectType = Range<PointType>;
	using RectArrayType= std::vector<RectType>;

	struct BoxData {
		RectArrayType m_BlockBoundArray;
		RectType      m_TotalBound;
	};

	const BoxData& GetBoxData(tile_id t) const
	{
		if (!IsDefined(t))
		{
			assert(m_BoxData.size() == 1);
			t = 0;
		}
		assert(t < m_BoxData.size());
		return m_BoxData[t];
	}

	PointBoundingBoxCache(const AbstrDataObject* featureData);
	const RectArrayType& GetBlockBoundArray(tile_id t) const { return GetBoxData(t).m_BlockBoundArray; }

	DRect GetTileBounds(tile_id t) const override
	{
		return Convert<DRect>(GetBoxData(t).m_TotalBound);
	}

	DRect GetBlockBounds(tile_id t, tile_offset blockNr) const override
	{
		assert(blockNr < GetBoxData(t).m_BlockBoundArray.size());
		return Convert<DRect>(GetBoxData(t).m_BlockBoundArray[blockNr]);
	}

	std::vector<BoxData> m_BoxData;
};


template <typename F>
struct SequenceBoundingBoxCache : AbstrBoundingBoxCache
{
	using PointType = Point<F>;
	using RectType = Range<PointType>;
	using RectArrayType = std::vector<RectType>;
	using PolygonType = typename sequence_traits<PointType>::container_type;

	struct BoxData {
		RectArrayType m_FeatBoundArray;
		RectArrayType m_BlockBoundArray;
		RectType      m_TotalBound;
	};

	const BoxData& GetBoxData(tile_id t) const
	{
		if (!IsDefined(t))
		{
			assert(m_BoxData.size() == 1);
			t = 0;
		}
		assert(t< m_BoxData.size());
		return m_BoxData[t];
	}

	SequenceBoundingBoxCache(const AbstrDataObject* featureData);
	const RectArrayType& GetBoundsArray    (tile_id t) const { return GetBoxData(t).m_FeatBoundArray ; }
	const RectArrayType& GetBlockBoundArray(tile_id t) const { return GetBoxData(t).m_BlockBoundArray; }

	DRect GetTileBounds(tile_id t) const override
	{
		return Convert<DRect>(GetBoxData(t).m_TotalBound);
	}

	DRect GetBounds(tile_id t, tile_offset featureID) const override
	{
		assert(featureID < GetBoxData(t).m_FeatBoundArray.size());
		return Convert<DRect>(GetBoxData(t).m_FeatBoundArray[featureID]);
	}

	DRect GetBlockBounds(tile_id t, tile_offset blockNr) const override
	{
		assert(blockNr < GetBoxData(t).m_BlockBoundArray.size());
		return Convert<DRect>(GetBoxData(t).m_BlockBoundArray[blockNr]);
	}

	std::vector<BoxData> m_BoxData;
};


template <typename F>
PointBoundingBoxCache<F>::PointBoundingBoxCache(const AbstrDataObject* featureData)
	: AbstrBoundingBoxCache(featureData)
{
	assert(featureData);

	auto da = const_array_cast<PointType>(featureData);
	assert(da);

	tile_id tn = featureData->GetTiledRangeData()->GetNrTiles();
	m_BoxData.resize(tn);

	parallel_tileloop(tn, [this, da](tile_id t)
		{
			auto data = da->GetTile(t);
			auto
				i = data.begin(),
				e = data.end();

			BoxData resultBoxes;
			resultBoxes.m_TotalBound = MakeBlockBoundArray(resultBoxes.m_BlockBoundArray, da->GetTile(t));

			m_BoxData[t] = std::move(resultBoxes);
		});
}

template <typename F>
SequenceBoundingBoxCache<F>::SequenceBoundingBoxCache(const AbstrDataObject* featureData)
	:	AbstrBoundingBoxCache(featureData)
{
	assert(featureData);

	auto da = const_array_cast<PolygonType>(featureData);
	assert(da);

	tile_id tn = featureData->GetTiledRangeData()->GetNrTiles();
	m_BoxData.resize(tn);

	parallel_tileloop(tn, [this, da](tile_id t)
	{
		auto data = da->GetTile(t);
				
		BoxData resultBoxes;
		RectArrayType& featBoundArray = resultBoxes.m_FeatBoundArray;
		featBoundArray.resize(data.size());
		auto ri = featBoundArray.begin();
		for (auto i = data.begin(), e = data.end(); i != e; ++ri, ++i)
			*ri = RangeFromSequence(i->begin(), i->end());

		resultBoxes.m_TotalBound = MakeBlockBoundArray(resultBoxes.m_BlockBoundArray, featBoundArray);

		m_BoxData[t] = std::move(resultBoxes);
	});
}

//----------------------------------------------------------------------
// class  : GetBoundingBoxCache
//----------------------------------------------------------------------

template <typename ScalarType>
const SequenceBoundingBoxCache<ScalarType>*
GetSequenceBoundingBoxCache(SharedPtr<const AbstrBoundingBoxCache>& bbCacheSlot, WeakPtr<const AbstrDataItem> featureAttr, bool mustPrepare)
{
	assert(featureAttr);
	leveled_critical_section::scoped_lock lock(cs_BB);
	DataReadLock readLock(featureAttr);
	assert(featureAttr->GetDataRefLockCount() > 0);

	const AbstrDataObject* featureData = featureAttr->GetCurrRefObj();
	auto& bbCache = g_BB_Register[featureData];
	if (!bbCache)
	{
		auto bbPtr = std::make_unique<SequenceBoundingBoxCache<ScalarType>> (featureData);
		bbPtr->Register(); // remove from global cache upon destruction
		bbCache = bbPtr.release();
	}
	bbCacheSlot = bbCache; // assign (shared) ownership to provided slot
	return debug_cast<const SequenceBoundingBoxCache<ScalarType>*>(bbCache);
}

template <typename ScalarType>
const PointBoundingBoxCache<ScalarType>*
GetPointBoundingBoxCache(SharedPtr<const AbstrBoundingBoxCache>& bbCacheSlot, WeakPtr<const AbstrDataItem> featureAttr, bool mustPrepare)
{
	assert(featureAttr);
	leveled_critical_section::scoped_lock lock(cs_BB);
	DataReadLock readLock(featureAttr);
	assert(featureAttr->GetDataRefLockCount() > 0);

	const AbstrDataObject* featureData = featureAttr->GetCurrRefObj();
	auto& bbCache = g_BB_Register[featureData];
	if (!bbCache)
	{
		auto bbPtr = std::make_unique<PointBoundingBoxCache<ScalarType>>(featureData);
		bbPtr->Register(); // remove from global cache upon destruction
		bbCache = bbPtr.release();
	}
	bbCacheSlot = bbCache; // assign (shared) ownership to provided slot
	return debug_cast<const PointBoundingBoxCache<ScalarType>*>(bbCache);
}

#endif // __GEO_BOUNDINGBOXCACHE_H
