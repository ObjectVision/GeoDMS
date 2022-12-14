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
#pragma once

#if !defined(__TIC_TILEFUNCTORIMPL_H)
#define __TIC_TILEFUNCTORIMPL_H

#include "DataArray.h"

//----------------------------------------------------------------------
// class  : FutureTileFunctor
//----------------------------------------------------------------------
#if defined(MG_DEBUG)
#define MG_DEBUG_LAZYTILEFUNCTOR
#else // defined(MG_DEBUG)
#define MG_DEBUG_LAZYTILEFUNCTOR
#endif //defined(MG_DEBUG)

#include "DataLocks.h"
#include "dbg/DebugCast.h"
#include "dbg/SeverityType.h"
#include "mem/TileData.h"
#include "ser/VectorStream.h"

template <typename V>
struct DelayedTileFunctor : TileFunctor<V>
{
	using future_tile = TileFunctor<V>::future_tile;
	using locked_cseq_t = TileFunctor<V>::locked_cseq_t;

	using cache_t = std::unique_ptr<SharedPtr<future_tile>[]>;
	cache_t m_ActiveTiles;

	DelayedTileFunctor(const AbstrTileRangeData* tiledDomainRangeData, range_data_ptr_or_void<field_of_t<V>> valueRangePtr, tile_id tn MG_DEBUG_ALLOCATOR_SRC_ARG)
		: TileFunctor<V>(tiledDomainRangeData, valueRangePtr MG_DEBUG_ALLOCATOR_SRC_PARAM)
		, m_ActiveTiles(std::make_unique<SharedPtr<future_tile>[]>(tn))
	{
		MG_CHECK(tiledDomainRangeData);
		MG_CHECK(tiledDomainRangeData->GetNrTiles() == tn);
		if constexpr (has_var_range_field_v<V>)
		{
			MG_CHECK(valueRangePtr);
		}
	}

	auto GetFutureTile(tile_id t) const->SharedPtr<future_tile> override
	{
		return m_ActiveTiles[t];
	}
	auto GetTile(tile_id t) const->locked_cseq_t override
	{
		dms_assert(t < this->GetTiledRangeData()->GetNrTiles());

		return m_ActiveTiles[t]->GetTile();
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
				reallocSO(resData, tf.tileSize, MustZero MG_DEBUG_ALLOCATOR_SRC("md_SrcStr"));
				tf.aFunc(GetSeq(resData), tf.pState);
				m_State.emplace<1>(std::move(resData));
			}
			dms_assert(m_State.index() == 1);
			return locked_cseq_t(this, GetConstSeq(std::get<1>(m_State)));
		}
		std::mutex  m_Mutex;
		future_state m_State;
#if defined(MG_DEBUG_ALLOCATOR)
		SharedStr md_SrcStr;
#endif
	};

	FutureTileFunctor(const AbstrTileRangeData* tiledDomainRangeData, range_data_ptr_or_void<field_of_t<V>> valueRangePtr, tile_id tn, PrepareFunc&& pFunc, ApplyFunc&& aFunc MG_DEBUG_ALLOCATOR_SRC_ARG)
		: DelayedTileFunctor<V>(tiledDomainRangeData, valueRangePtr, tn MG_DEBUG_ALLOCATOR_SRC_PARAM)
	{
		dms_assert(tn > 1);
		for (tile_id t = 0; t != tn; ++t)
			this->m_ActiveTiles[t] = new tile_record(pFunc(t), aFunc, tiledDomainRangeData->GetTileSize(t) MG_DEBUG_ALLOCATOR_SRC_PARAM);
	}
};

template <typename V, typename PrepareState, bool MustZero, typename PrepareFunc, typename ApplyFunc>
auto make_unique_FutureTileFunctor(const AbstrTileRangeData* tiledDomainRangeData, range_data_ptr_or_void<field_of_t<V>> valueRangePtr, tile_id tn, PrepareFunc&& pFunc, ApplyFunc&& aFunc MG_DEBUG_ALLOCATOR_SRC_ARG)
{ 
	return std::make_unique<FutureTileFunctor<V, PrepareState, MustZero, PrepareFunc, ApplyFunc>>(tiledDomainRangeData, valueRangePtr, tn, std::forward<PrepareFunc>(pFunc), std::forward<ApplyFunc>(aFunc) MG_DEBUG_ALLOCATOR_SRC_PARAM);
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
		std::mutex                    m_Mutex;
		std::shared_ptr< tile_data >  m_TileFutureSPtr; // keep it !
	};

	using cache_t = std::unique_ptr<lazy_tile_record[]>;

	LazyTileFunctor(const AbstrTileRangeData* tiledDomainRangeData, range_data_ptr_or_void<field_of_t<V>> valueRangePtr, tile_id tn, ApplyFunc&& aFunc MG_DEBUG_ALLOCATOR_SRC_ARG)
		: GeneratedTileFunctor<V>(tiledDomainRangeData, valueRangePtr MG_DEBUG_ALLOCATOR_SRC_PARAM)
		, m_ApplyFunc(std::move(aFunc))
		, m_ActiveTiles(std::make_unique<lazy_tile_record[]>(tn))
	{}

//	auto CreateFutureTile(tile_id t) const->TileRef override;
	auto GetWritableTile(tile_id t, dms_rw_mode rwMode)->locked_seq_t override;
	auto GetTile(tile_id t) const->locked_cseq_t override;

	ApplyFunc m_ApplyFunc;
	mutable cache_t m_ActiveTiles;
};

template <typename V, typename ApplyFunc>
auto make_unique_LazyTileFunctor(const AbstrTileRangeData* tiledDomainRangeData, range_data_ptr_or_void<field_of_t<V>> valueRangePtr, tile_id tn, ApplyFunc&& aFunc MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	return std::make_unique<LazyTileFunctor<V, ApplyFunc>>(tiledDomainRangeData, valueRangePtr, tn, std::forward<ApplyFunc>(aFunc) MG_DEBUG_ALLOCATOR_SRC_PARAM);
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

	auto tileSPtr = m_ActiveTiles[t].m_TileFutureSPtr;
	dms_assert(tileSPtr); // called only from within m_Func
	dms_assert(tileSPtr->size() == this->GetTiledRangeData()->GetTileSize(t));

	return locked_seq_t(make_SharedThing(std::move(tileSPtr)), GetSeq(*tileSPtr));
}

template <typename V, typename ApplyFunc>
auto LazyTileFunctor<V, ApplyFunc>::GetTile(tile_id t) const -> locked_cseq_t
{
	dms_assert(t < this->GetTiledRangeData()->GetNrTiles());

	auto lock = std::scoped_lock(m_ActiveTiles[t].m_Mutex);

	auto tileSPtr = m_ActiveTiles[t].m_TileFutureSPtr;
	if (!tileSPtr)
	{
		tileSPtr = std::make_shared<tile_data>(); // done by GetWritableTile

		resizeSO(*tileSPtr, this->GetTiledRangeData()->GetTileSize(t), false MG_DEBUG_ALLOCATOR_SRC("this->md_SrcStr"));

		m_ActiveTiles[t].m_TileFutureSPtr = tileSPtr;
		m_ApplyFunc(const_cast<LazyTileFunctor<V, ApplyFunc>*>(this), t);
	}
	dms_assert(tileSPtr);
	dms_assert(tileSPtr->size() == this->GetTiledRangeData()->GetTileSize(t));

	return locked_cseq_t(make_SharedThing(std::move(tileSPtr)), GetConstSeq(*tileSPtr) );
}

//----------------------------------------------------------------------
// Building blocks for LazyTileFunctor heristics
//----------------------------------------------------------------------

inline UInt32 ElementWeight(const AbstrDataItem* adi)
{
	return 0;

	if (adi->HasVoidDomainGuarantee())
		return 0;
	auto bitSize = adi->GetAbstrValuesUnit()->GetValueType()->GetBitSize(); // bool => 1; UInt32 => 32; DPoint == 128
	if (!bitSize)
		return 256; // string weight
	if (adi->GetValueComposition() != ValueComposition::Single)
		return bitSize * 32; // Sequence<UInt8> -> 256 too
	return  bitSize;
}


#endif //!defined(__TIC_TILEFUNCTORIMPL_H)


