// Copyright (C) 1998-2023 Object Vision b.v. 
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
#include "dbg/SeverityType.h"
#include "mem/TileData.h"
#include "ptr/OwningPtrReservedArray.h"
#include "ptr/WeakPtr.h"
#include "ser/VectorStream.h"

template <typename V>
struct DelayedTileFunctor : TileFunctor<V>
{
	using future_tile = TileFunctor<V>::future_tile;
	using locked_cseq_t = TileFunctor<V>::locked_cseq_t;

	using cache_t = std::unique_ptr<std::shared_ptr<future_tile>[]>;
	cache_t m_ActiveTiles;
	// Non-owning back-ref to the owning result item, used only to propagate FR_Data failures. The result
	// OWNS this functor (via its m_DataObject), so this back-ref must not own the result back (that self-cycle
	// pinned both). Cleared by ImLosingIt() when the item releases this object (AbstrDataItem::ClearDataObject).
	mutable std::weak_ptr<AbstrDataItem> m_ResultAdi; // std::weak_ptr-backed non-owning back-ref (lock at use)

	DelayedTileFunctor(SharedMutableDataItem resultAdi, const AbstrTileRangeData* tiledDomainRangeData, range_data_ptr_or_void<field_of_t<V>> valueRangePtr MG_DEBUG_ALLOCATOR_SRC(SharedStr srcStr))
		: TileFunctor<V>(tiledDomainRangeData, valueRangePtr MG_DEBUG_ALLOCATOR_SRC_PARAM)
		, m_ResultAdi(resultAdi)
		, m_ActiveTiles(std::make_unique<std::shared_ptr<future_tile>[]>(tiledDomainRangeData->GetNrTiles()))
	{
		MG_CHECK(tiledDomainRangeData);
		MG_CHECK(tiledDomainRangeData->GetNrTiles() == tiledDomainRangeData->GetNrTiles());
		if constexpr (has_var_range_field_v<V>)
		{
			MG_CHECK(valueRangePtr);
		}
	}

	auto GetFutureTile(tile_id t) const -> std::shared_ptr<future_tile> override
	{
		return m_ActiveTiles[t];
	}
	// m_ResultAdi is non-owning: if the item already let go of this object it is null -> nothing to report to.
	void ImLosingIt() const override { m_ResultAdi.reset(); }
	auto GetTile(tile_id t) const->locked_cseq_t override
	{
		if (auto resultAdi = m_ResultAdi.lock(); resultAdi && resultAdi->WasFailed(FailType::Data))
			resultAdi->ThrowFail();
		try
		{
			assert(t < this->GetTiledRangeData()->GetNrTiles());

			return GetFutureTile(t)->GetTile();
		}
		catch (...)
		{
			if (auto resultAdi = m_ResultAdi.lock())
				resultAdi->CatchFail(FailType::Data);
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
		tile_record(PrepareState ps, const ApplyFunc& aFunc, tile_offset tileSize MG_DEBUG_ALLOCATOR_SRC(SharedStr srcStr))
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
				reallocSO(resData, tf.tileSize, MustZero MG_DEBUG_ALLOCATOR_SRC(md_SrcStr.c_str()));
				tf.aFunc(GetSeq(resData), tf.pState);
					m_State.template emplace<1>(std::move(resData));
			}
			assert(m_State.index() == 1);
			return locked_cseq_t(this->shared_from_this(), GetConstSeq(std::get<1>(m_State)));
		}
		std::mutex  m_Mutex;
		future_state m_State;
#if defined(MG_DEBUG_ALLOCATOR)
		SharedStr md_SrcStr;
#endif
	};

	// Computed on first demand, then kept: the tile_record's variant never flips back.
	materialization GetMaterialization() const override { return materialization::deferred; }

	FutureTileFunctor(SharedMutableDataItem resultAdi, const AbstrTileRangeData* tiledDomainRangeData, range_data_ptr_or_void<field_of_t<V>> valueRangePtr
	, PrepareFunc&& pFunc_, ApplyFunc&& aFunc_ MG_DEBUG_ALLOCATOR_SRC(SharedStr srcStr))
	: DelayedTileFunctor<V>(resultAdi, tiledDomainRangeData, valueRangePtr MG_DEBUG_ALLOCATOR_SRC_PARAM)
	, aFunc(aFunc_)
	{
		assert(tiledDomainRangeData->GetNrTiles() > 1);
		for (tile_id t = 0; t != tiledDomainRangeData->GetNrTiles(); ++t)
			this->m_ActiveTiles[t] = std::make_shared<tile_record>(pFunc_(t), aFunc, tiledDomainRangeData->GetTileSize(t) MG_DEBUG_ALLOCATOR_SRC(this->md_SrcStr));
	}

	ApplyFunc aFunc;
};

template <typename V, typename PrepareState, bool MustZero, typename PrepareFunc, typename ApplyFunc>
auto make_unique_FutureTileFunctor(SharedMutableDataItem resultAdi, bool lazy, const AbstrTileRangeData* tiledDomainRangeData, range_data_ptr_or_void<field_of_t<V>> valueRangePtr, PrepareFunc&& pFunc, ApplyFunc&& aFunc MG_DEBUG_ALLOCATOR_SRC(SharedStr srcStr))
-> std::unique_ptr<TileFunctor<V>>
{ 
	if (lazy)
	{
		auto tn = tiledDomainRangeData->GetNrTiles();
		auto preparedStates = OwningPtrReservedArray<PrepareState>(tn);
		for (tile_id t = 0; t != tn; ++t)
			preparedStates.emplace_back(pFunc(t));

		// Deliberately no rwMode here: LazyTileFunctor::GetTile has already allocated the tile by the
		// time this runs, so a mode passed now could not zero anything -- it only looked as if it
		// could, and MustZero was being silently dropped. It now travels as the functor's template
		// argument down to that allocation, mirroring the eager FutureTileFunctor path below.
		auto lazyApplyFunc = [aFunc, preparedStates = std::move(preparedStates)](AbstrDataObject* ado, tile_id t)
		{
			aFunc(mutable_array_cast<V>(ado)->GetWritableTile(t), preparedStates[t]);
		};

		return make_unique_LazyTileFunctor<V, MustZero>(resultAdi, tiledDomainRangeData, valueRangePtr, std::move(lazyApplyFunc) MG_DEBUG_ALLOCATOR_SRC(std::move(srcStr)));
	}

	return std::make_unique<FutureTileFunctor<V, PrepareState, MustZero, PrepareFunc, ApplyFunc>>(resultAdi,
		tiledDomainRangeData, valueRangePtr, 
		std::forward<PrepareFunc>(pFunc), 
		std::forward<ApplyFunc>(aFunc) 
		MG_DEBUG_ALLOCATOR_SRC(std::move(srcStr))
	);
}

//----------------------------------------------------------------------
// LazyTileFunctor
//----------------------------------------------------------------------
// MustZero is a template argument rather than a member for the same reason as in FutureTileFunctor:
// it decides how GetTile allocates each tile, costs no runtime storage, and cannot then be
// contradicted by an rwMode handed to GetWritableTile after the fact.
template <typename V, bool MustZero, typename ApplyFunc>
struct LazyTileFunctor : GeneratedTileFunctor<V>
{
	using typename TileFunctor<V>::locked_cseq_t;
	using typename TileFunctor<V>::locked_seq_t;

	struct tile_data : sequence_traits<V>::tile_container_type {};

	struct lazy_tile_record
	{
		std::mutex                  m_Mutex;
		std::weak_ptr< tile_data >  m_TileFutureWPtr; // don't keep it !
	};

	using cache_t = std::unique_ptr<lazy_tile_record[]>;

	LazyTileFunctor(SharedMutableDataItem resultAdi, const AbstrTileRangeData* tiledDomainRangeData, range_data_ptr_or_void<field_of_t<V>> valueRangePtr, ApplyFunc&& aFunc MG_DEBUG_ALLOCATOR_SRC(SharedStr srcStr))
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
	// Tiles are held weakly ("don't keep it !"): freed at the last consumer release, recomputed on
	// the next request, which cascades into suppliers that are themselves lazy.
	materialization GetMaterialization() const override { return materialization::streaming; }
	// m_ResultAdi is non-owning: cleared when the item releases this object (AbstrDataItem::ClearDataObject).
	void ImLosingIt() const override { m_ResultAdi.reset(); }

	ApplyFunc m_ApplyFunc;
	mutable cache_t m_ActiveTiles;
	// Non-owning back-ref to the owning result item (failure propagation only); see DelayedTileFunctor.
	mutable std::weak_ptr<AbstrDataItem> m_ResultAdi; // std::weak_ptr-backed non-owning back-ref (lock at use)
};

// MustZero defaults to false to keep the four sites that construct a LazyTileFunctor directly
// (ID.cpp, OperUnit.cpp, CastedUnaryAttrOper.h, AbstrDataItem.cpp) at today's behaviour; only
// make_unique_FutureTileFunctor's lazy branch passes it explicitly.
template <typename V, bool MustZero = false, typename ApplyFunc>
auto make_unique_LazyTileFunctor(SharedMutableDataItem resultAdi, const AbstrTileRangeData* tiledDomainRangeData, range_data_ptr_or_void<field_of_t<V>> valueRangePtr, ApplyFunc&& aFunc MG_DEBUG_ALLOCATOR_SRC(SharedStr srcStr))
{
	return std::make_unique<LazyTileFunctor<V, MustZero, ApplyFunc>>(resultAdi, tiledDomainRangeData, valueRangePtr, std::forward<ApplyFunc>(aFunc) MG_DEBUG_ALLOCATOR_SRC(std::move(srcStr)));
}


//----------------------------------------------------------------------
// LazyTileFunctor impl
//----------------------------------------------------------------------
#include "mem/HeapSequenceProvider.h"

template <typename V, bool MustZero, typename ApplyFunc>
auto LazyTileFunctor<V, MustZero, ApplyFunc>::GetWritableTile(tile_id t, dms_rw_mode rwMode) -> locked_seq_t
{
	assert(t < this->GetTiledRangeData()->GetNrTiles());

//	auto lock = std::scoped_lock(m_ActiveTiles[t].m_Mutex);

	auto tileSPtr = m_ActiveTiles[t].m_TileFutureWPtr.lock();
	assert(tileSPtr); // called only from within m_Func
	assert(tileSPtr->size() == this->GetTiledRangeData()->GetTileSize(t));

	// Symmetric with HeapSingleArray::GetWritableTile: the tile was already allocated (by GetTile,
	// from MustZero), so rwMode cannot zero anything here. An apply-func asking for a zeroed tile from
	// a functor instantiated MustZero=false would silently accumulate into indeterminate memory --
	// the shape of issue #1169. Only for fixed-size elements; sequence/string elements are
	// raw_construct-ed regardless. Reachable via the four direct make_unique_LazyTileFunctor callers,
	// whose apply-funcs choose their own mode; the FutureTileFunctor branch passes none.
	if constexpr (has_fixed_elem_size_v<V>)
		MGD_CHECK_OBJ(MustZero || rwMode != dms_rw_mode::write_only_mustzero);

	return locked_seq_t(std::static_pointer_cast<void>(tileSPtr), GetSeq(*tileSPtr));
}

template <typename V, bool MustZero, typename ApplyFunc>
auto LazyTileFunctor<V, MustZero, ApplyFunc>::GetTile(tile_id t) const -> locked_cseq_t
{
	if (auto resultAdi = m_ResultAdi.lock(); resultAdi && resultAdi->WasFailed(FailType::Data))
		resultAdi->ThrowFail();
	try {
		assert(t < this->GetTiledRangeData()->GetNrTiles());

		auto lock = std::scoped_lock(m_ActiveTiles[t].m_Mutex);

		auto tileSPtr = m_ActiveTiles[t].m_TileFutureWPtr.lock();
		if (!tileSPtr)
		{
			tileSPtr = std::make_shared<tile_data>(); // done by GetWritableTile

			// THE allocation site for a lazy tile, so MustZero must be honoured here -- it was
			// hardcoded false, which dropped the MustZero of every make_unique_FutureTileFunctor
			// that took the lazy branch. (md_SrcStr was passed as a string literal, not its value.)
			resizeSO(*tileSPtr, this->GetTiledRangeData()->GetTileSize(t), MustZero MG_DEBUG_ALLOCATOR_SRC(this->md_SrcStr.c_str()));

			m_ActiveTiles[t].m_TileFutureWPtr = tileSPtr;
			m_ApplyFunc(const_cast<LazyTileFunctor<V, MustZero, ApplyFunc>*>(this), t);
		}
		assert(tileSPtr);
		assert(tileSPtr->size() == this->GetTiledRangeData()->GetTileSize(t));

		return locked_cseq_t(std::static_pointer_cast<const void>(tileSPtr), GetConstSeq(*tileSPtr));
	}
	catch (...)
	{
		if (auto resultAdi = m_ResultAdi.lock())
			resultAdi->CatchFail(FailType::Data);
		throw;
	}
}

#endif //!defined(__TIC_TILEFUNCTORIMPL_H)


