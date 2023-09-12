// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__TIC_TILEFUNCTORIMPL_H)
#define __TIC_TILEFUNCTORIMPL_H

#include "DataArray.h"

//----------------------------------------------------------------------
// class  : FutureTileFunctor
//----------------------------------------------------------------------

#include "DataLocks.h"
//#include "dbg/DebugCast.h"
#include "dbg/SeverityType.h"
#include "mem/TileData.h"
#include "ptr/OwningPtrReservedArray.h"
#include "ser/VectorStream.h"

template <typename V>
struct DelayedTileFunctor : TileFunctor<V>
{
	using future_tile = TileFunctor<V>::future_tile;
	using locked_cseq_t = TileFunctor<V>::locked_cseq_t;

	using cache_t = std::unique_ptr<SharedPtr<future_tile>[]>;
	cache_t m_ActiveTiles;
	SharedPtr<AbstrDataItem> m_ResultAdi;

	DelayedTileFunctor(SharedPtr<AbstrDataItem> resultAdi, const AbstrTileRangeData* tiledDomainRangeData, range_data_ptr_or_void<field_of_t<V>> valueRangePtr MG_DEBUG_ALLOCATOR_SRC_ARG)
		: TileFunctor<V>(tiledDomainRangeData, valueRangePtr MG_DEBUG_ALLOCATOR_SRC_PARAM)
		, m_ResultAdi(resultAdi)
		, m_ActiveTiles(std::make_unique<SharedPtr<future_tile>[]>(tiledDomainRangeData->GetNrTiles()))
	{
		MG_CHECK(tiledDomainRangeData);
		MG_CHECK(tiledDomainRangeData->GetNrTiles() == tiledDomainRangeData->GetNrTiles());
		if constexpr (has_var_range_field_v<V>)
		{
			MG_CHECK(valueRangePtr);
		}
	}

	auto GetFutureTile(tile_id t) const -> SharedPtr<future_tile> override
	{
		return m_ActiveTiles[t];
	}
	auto GetTile(tile_id t) const->locked_cseq_t override
	{
		if (m_ResultAdi->WasFailed(FR_Data))
			m_ResultAdi->ThrowFail();
		try
		{
			assert(t < this->GetTiledRangeData()->GetNrTiles());

			return GetFutureTile(t)->GetTile();
		}
		catch (...)
		{
			m_ResultAdi->CatchFail(FailType::FR_Data);
			throw;
		}
	}
};

template <typename V, typename PrepareState, bool MustZero, typename PrepareFunc, typename ApplyFunc>
struct FutureTileFunctor : DelayedTileFunctor<V>
{
	using typename DelayedTileFunctor<V>::locked_cseq_t;

	using future_tile = DelayedTileFunctor<V>::future_tile;

	using tile_data = typename sequence_traits<V>::tile_container_type;
	struct tile_spec {
		PrepareState pState;
		ApplyFunc    aFunc;
		tile_offset tileSize;
	};
	using future_state = std::variant<tile_spec, tile_data>;

	struct tile_record : future_tile
	{
		tile_record(PrepareState ps, const ApplyFunc& aFunc, tile_offset tileSize MG_DEBUG_ALLOCATOR_SRC_ARG)
			: m_State(std::in_place_index<0>, std::move(ps), aFunc, tileSize)
#if defined(MG_DEBUG_ALLOCATOR)
			, md_SrcStr(srcStr)
#endif
		{}
		auto GetTile() -> locked_cseq_t override
		{
			auto lock = std::scoped_lock(m_Mutex);
			if (m_State.index() == 0)
			{
				const tile_spec& tf = std::get<0>(m_State);
				tile_data resData;
				reallocSO(resData, tf.tileSize, MustZero MG_DEBUG_ALLOCATOR_SRC(md_SrcStr));
				tf.aFunc(GetSeq(resData), tf.pState);
				m_State.emplace<1>(std::move(resData));
			}
			assert(m_State.index() == 1);
			return locked_cseq_t(this, GetConstSeq(std::get<1>(m_State)));
		}
		std::mutex  m_Mutex;
		future_state m_State;
#if defined(MG_DEBUG_ALLOCATOR)
		CharPtr md_SrcStr;
#endif
	};

	FutureTileFunctor(SharedPtr<AbstrDataItem> resultAdi, const AbstrTileRangeData* tiledDomainRangeData, range_data_ptr_or_void<field_of_t<V>> valueRangePtr
	, PrepareFunc&& pFunc_, ApplyFunc&& aFunc_ MG_DEBUG_ALLOCATOR_SRC_ARG)
	: DelayedTileFunctor<V>(resultAdi, tiledDomainRangeData, valueRangePtr MG_DEBUG_ALLOCATOR_SRC_PARAM)
	, aFunc(aFunc_)
#if defined(MG_DEBUG_ALLOCATOR)
	, md_SrcStr(srcStr)
#endif
	{
		assert(tiledDomainRangeData->GetNrTiles() > 1);
		for (tile_id t = 0; t != tiledDomainRangeData->GetNrTiles(); ++t)
			this->m_ActiveTiles[t] = new tile_record(pFunc_(t), aFunc, tiledDomainRangeData->GetTileSize(t) MG_DEBUG_ALLOCATOR_SRC_PARAM);
	}

	ApplyFunc aFunc;

#if defined(MG_DEBUG_ALLOCATOR)
	CharPtr md_SrcStr;
#endif
};

template <typename V, typename PrepareState, bool MustZero, typename PrepareFunc, typename ApplyFunc>
auto make_unique_FutureTileFunctor(SharedPtr<AbstrDataItem> resultAdi, bool lazy, const AbstrTileRangeData* tiledDomainRangeData, range_data_ptr_or_void<field_of_t<V>> valueRangePtr, PrepareFunc&& pFunc, ApplyFunc&& aFunc MG_DEBUG_ALLOCATOR_SRC_ARG)
-> std::unique_ptr<TileFunctor<V>>
{ 
	if (lazy)
	{
		auto tn = tiledDomainRangeData->GetNrTiles();
		auto preparedStates = OwningPtrReservedArray<PrepareState>(tn);
		for (tile_id t = 0; t != tn; ++t)
			preparedStates.emplace_back(pFunc(t));

		auto lazyApplyFunc = [aFunc, preparedStates = std::move(preparedStates)](AbstrDataObject* ado, tile_id t)
		{
			auto rwMode = MustZero ? dms_rw_mode::write_only_mustzero : dms_rw_mode::write_only_all;
			aFunc(mutable_array_cast<V>(ado)->GetWritableTile(t, rwMode), preparedStates[t]);
		};

		return make_unique_LazyTileFunctor<V>(resultAdi, tiledDomainRangeData, valueRangePtr, std::move(lazyApplyFunc) MG_DEBUG_ALLOCATOR_SRC_PARAM);
	}

	return std::make_unique<FutureTileFunctor<V, PrepareState, MustZero, PrepareFunc, ApplyFunc>>(resultAdi,
		tiledDomainRangeData, valueRangePtr, 
		std::forward<PrepareFunc>(pFunc), 
		std::forward<ApplyFunc>(aFunc) 
		MG_DEBUG_ALLOCATOR_SRC_PARAM
	);
}

//----------------------------------------------------------------------
// LazyTileFunctor
//----------------------------------------------------------------------
template <typename V, typename ApplyFunc>
struct LazyTileFunctor : GeneratedTileFunctor<V>
{
	using typename TileFunctor<V>::locked_cseq_t;
	using typename TileFunctor<V>::locked_seq_t;
	using tile_data = typename sequence_traits<V>::tile_container_type;

	struct lazy_tile_record
	{
		std::mutex                  m_Mutex;
		std::weak_ptr< tile_data >  m_TileFutureWPtr; // don't keep it !
	};

	using cache_t = std::unique_ptr<lazy_tile_record[]>;

	LazyTileFunctor(SharedPtr<AbstrDataItem> resultAdi, const AbstrTileRangeData* tiledDomainRangeData, range_data_ptr_or_void<field_of_t<V>> valueRangePtr, ApplyFunc&& aFunc MG_DEBUG_ALLOCATOR_SRC_ARG)
		: GeneratedTileFunctor<V>(tiledDomainRangeData, valueRangePtr MG_DEBUG_ALLOCATOR_SRC_PARAM)
		, m_ApplyFunc(std::move(aFunc))
		, m_ActiveTiles(std::make_unique<lazy_tile_record[]>(tiledDomainRangeData->GetNrTiles()))
		, m_ResultAdi(resultAdi)
	{
		assert(resultAdi);
	}

//	auto CreateFutureTile(tile_id t) const->TileRef override;
	auto GetWritableTile(tile_id t, dms_rw_mode rwMode)->locked_seq_t override;
	auto GetTile(tile_id t) const->locked_cseq_t override;

	ApplyFunc m_ApplyFunc;
	mutable cache_t m_ActiveTiles;
	SharedPtr<AbstrDataItem> m_ResultAdi;
};

template <typename V, typename ApplyFunc>
auto make_unique_LazyTileFunctor(SharedPtr<AbstrDataItem> resultAdi, const AbstrTileRangeData* tiledDomainRangeData, range_data_ptr_or_void<field_of_t<V>> valueRangePtr, ApplyFunc&& aFunc MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	return std::make_unique<LazyTileFunctor<V, ApplyFunc>>(resultAdi, tiledDomainRangeData, valueRangePtr, std::forward<ApplyFunc>(aFunc) MG_DEBUG_ALLOCATOR_SRC_PARAM);
}


//----------------------------------------------------------------------
// LazyTileFunctor impl
//----------------------------------------------------------------------
#include "mem/HeapSequenceProvider.h"

template <typename V, typename ApplyFunc>
auto LazyTileFunctor<V, ApplyFunc>::GetWritableTile(tile_id t, dms_rw_mode rwMode) -> locked_seq_t
{
	dms_assert(t < this->GetTiledRangeData()->GetNrTiles());

//	auto lock = std::scoped_lock(m_ActiveTiles[t].m_Mutex);

	auto tileSPtr = m_ActiveTiles[t].m_TileFutureWPtr.lock();
	dms_assert(tileSPtr); // called only from within m_Func
	dms_assert(tileSPtr->size() == this->GetTiledRangeData()->GetTileSize(t));

	return locked_seq_t(make_SharedThing(std::move(tileSPtr)), GetSeq(*tileSPtr));
}

template <typename V, typename ApplyFunc>
auto LazyTileFunctor<V, ApplyFunc>::GetTile(tile_id t) const -> locked_cseq_t
{
	if (m_ResultAdi->WasFailed(FR_Data))
		m_ResultAdi->ThrowFail();
	try {
		assert(t < this->GetTiledRangeData()->GetNrTiles());

		auto lock = std::scoped_lock(m_ActiveTiles[t].m_Mutex);

		auto tileSPtr = m_ActiveTiles[t].m_TileFutureWPtr.lock();
		if (!tileSPtr)
		{
			tileSPtr = std::make_shared<tile_data>(); // done by GetWritableTile

			resizeSO(*tileSPtr, this->GetTiledRangeData()->GetTileSize(t), false MG_DEBUG_ALLOCATOR_SRC("this->md_SrcStr"));

			m_ActiveTiles[t].m_TileFutureWPtr = tileSPtr;
			m_ApplyFunc(const_cast<LazyTileFunctor<V, ApplyFunc>*>(this), t);
		}
		assert(tileSPtr);
		assert(tileSPtr->size() == this->GetTiledRangeData()->GetTileSize(t));

		return locked_cseq_t(make_SharedThing(std::move(tileSPtr)), GetConstSeq(*tileSPtr));
	}
	catch (...)
	{
		m_ResultAdi->CatchFail(FR_Data);
		throw;
	}
}

#endif //!defined(__TIC_TILEFUNCTORIMPL_H)


