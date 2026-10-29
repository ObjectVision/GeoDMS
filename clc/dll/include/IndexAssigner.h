// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#include "ClcBase.h"

#if !defined(__CLC_INDEXASSIGNER_H)
#define __CLC_INDEXASSIGNER_H 

#include "RtcBase.h"

#include "UnitProcessor.h"
#include "mem/tiledata.h"
#include "vt/CheckedCalc.h"
#include "mci/ValueClass.h"
#include "mci/ValueClassID.h"
#include "mci/ValueWrap.h"

/******************************************************************************/

template<typename IndexType = UInt64>
struct IndexAssignerBase : UnitProcessor
{
	IndexAssignerBase(AbstrDataItem* resItem, AbstrDataObject* res, tile_id t, SizeT start, SizeT len, IndexType* indices = nullptr)
		:	m_ResItem(resItem)
		,	m_Res(res)
		,	m_TileID(t)
		,	m_Indices(indices)
		,	m_Start(start)
		,	m_Len(len)
	{
		if (indices)
			return;

		if (res->GetValuesType()->GetValueClassID() == ValueWrap<IndexType>::GetStaticClass()->GetValueClassID() && res->IsFirstValueZero())
		{
			m_IndexHolder = mutable_array_dynacast<IndexType>(res)->GetDataWrite(t, dms_rw_mode::read_write);
			m_Indices = m_IndexHolder.begin() + start;
		}
		else
		{
			m_ResBuffer.reset( new IndexType[len] );
			m_Indices = m_ResBuffer.get();
		}
	}
	void Store()
	{
		if (m_ResBuffer)
			m_ResItem->GetAbstrValuesUnit()->InviteUnitProcessor(*this);
	}

	template <typename E>
	void VisitImpl(const Unit<E>* inviter) const
	{
		auto resData = mutable_array_cast<E>(m_Res)->GetDataWrite(m_TileID, dms_rw_mode::read_write);
		SizeT sz = resData.size();

		assert(m_Start         <= sz);
		assert(m_Start + m_Len <= sz);
		assert(m_Len           <= sz);

		Range_Index2Value_checked(inviter->GetRange(), m_Indices, m_Indices + m_Len, resData.begin()+m_Start);
	}

	AbstrDataItem*   m_ResItem;
	AbstrDataObject* m_Res;
	tile_id          m_TileID;

	DataArrayBase<IndexType>::locked_seq_t m_IndexHolder;
	IndexType*       m_Indices;
	SizeT            m_Start, m_Len;

	std::unique_ptr<IndexType[]> m_ResBuffer;
};

using IndexAssigner32    = tl::fold_t<typelists::domain_elements, IndexAssignerBase<UInt32>, UnitVisitorImpl>;
using IndexAssignerSizeT = tl::fold_t<typelists::domain_elements, IndexAssignerBase<SizeT>, UnitVisitorImpl>;

/******************************************************************************/

// #1327: gives a relation the values of a chain of index tiles (tile_appender::TakeTiles in TileChannel.h), after its
// domain got the count of the chain. A relation whose values have the index type and start at zero takes the tiles
// themselves; any other one gets the value of each index in its range, tile by tile, and each index tile is freed as
// soon as it is converted. Invite it on the values unit of the relation.
template<typename IndexType>
struct IndexTileAssignerBase : UnitProcessor
{
	using tiles_t = std::vector<std::shared_ptr<tile<IndexType>>>;

	IndexTileAssignerBase(AbstrDataObject* res, tiles_t& tiles)
		:	m_Res(res)
		,	m_Tiles(tiles)
	{}

	template <typename E>
	void VisitImpl(const Unit<E>* inviter) const
	{
		auto resArray = mutable_array_cast<E>(m_Res);
		if (m_Tiles.empty())
		{
			MG_CHECK(resArray->GetTiledRangeData()->GetElemCount() == 0); // an empty domain can have one tile of no elements
			return;
		}
		tile_id tn = resArray->GetTiledRangeData()->GetNrTiles();
		MG_CHECK(tn == m_Tiles.size());
		if constexpr (std::is_same_v<E, IndexType>)
			if (m_Res->IsFirstValueZero())
			{
				for (tile_id t = 0; t != tn; ++t)
					resArray->AdoptTile(t, std::move(m_Tiles[t]));
				return;
			}
		auto range = inviter->GetRange();
		for (tile_id t = 0; t != tn; ++t)
		{
			auto indices = GetConstSeq(*m_Tiles[t]);
			auto resData = resArray->GetWritableTile(t, dms_rw_mode::write_only_all);
			MG_CHECK(resData.size() == indices.size());
			Range_Index2Value_checked(range, indices.begin(), indices.end(), resData.begin());
			m_Tiles[t].reset();
		}
	}

	AbstrDataObject* m_Res;
	tiles_t&         m_Tiles;
};

template<typename IndexType>
using IndexTileAssigner = tl::fold_t<typelists::domain_elements, IndexTileAssignerBase<IndexType>, UnitVisitorImpl>;

/******************************************************************************/

struct IdAssignerBase : UnitProcessor
{
	IdAssignerBase(AbstrDataObject* res, tile_id t, SizeT start, SizeT base, SizeT len)
		:	m_Res(res)
		,	m_TileID(t)
		,	m_Start(start)
		,	m_Base(base)
		,	m_Len(len)
	{}

	template <typename E>
	void VisitImpl(const Unit<E>* inviter) const
	{
		auto resData = mutable_array_cast<E>(m_Res)->GetDataWrite(m_TileID, dms_rw_mode::read_write);
		SizeT sz = resData.size();

		assert(m_Start         <= sz);
		assert(m_Start + m_Len <= sz);
		assert(m_Len           <= sz);

		auto target = resData.begin() + m_Start;
		SizeT i = m_Base, e = m_Base + m_Len;

		typename Unit<E>::range_t range = inviter->GetRange();
		assert(i <= Cardinality(range));
		assert(e <= Cardinality(range));
		for (; i != e; ++target, ++i) 
			*target = Range_GetValue_naked(range, i);
	}

	AbstrDataObject* m_Res;
	tile_id        m_TileID;
	SizeT          m_Start, m_Base, m_Len;
};

using IdAssigner = tl::fold_t<typelists::domain_elements, IdAssignerBase, UnitVisitorImpl>;

/******************************************************************************/

struct NullAssignerBase : UnitProcessor
{
	NullAssignerBase(AbstrDataObject* res, tile_id t, SizeT start, SizeT len)
		:	m_Res(res)
		,	m_TileID(t)
		,	m_Start(start)
		,	m_Len(len)
	{}

	template <typename E>
	void VisitImpl(const Unit<E>* inviter) const
	{
		auto resData = mutable_array_cast<E>(m_Res)->GetDataWrite(m_TileID, dms_rw_mode::read_write);
		SizeT sz = resData.size();

		assert(m_Start         <= sz);
		assert(m_Start + m_Len <= sz);
		assert(m_Len           <= sz);

		auto first = resData.begin() + m_Start;
		fast_fill(first, first+ m_Len, UNDEFINED_OR_ZERO(E) );
	}

	AbstrDataObject* m_Res;
	tile_id        m_TileID;
	SizeT          m_Start, m_Len;
};

using NullAssigner = tl::fold_t<typelists::domain_elements, NullAssignerBase, UnitVisitorImpl>;

/******************************************************************************/

#endif // !defined(__CLC_INDEXASSIGNER_H)

