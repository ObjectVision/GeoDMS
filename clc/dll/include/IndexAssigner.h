// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#include "ClcPCH.h"

#if !defined(__CLC_INDEXASSIGNER_H)
#define __CLC_INDEXASSIGNER_H 

#include "RtcBase.h"

#include "UnitProcessor.h"
#include "geo/CheckedCalc.h"
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
		auto resData = composite_cast<DataArray<E>*>(m_Res)->GetDataWrite(m_TileID);
		SizeT sz = resData.size();

		assert(m_Start         <= sz);
		assert(m_Start + m_Len <= sz);
		assert(m_Len           <= sz);

		Range_Index2Value_checked(inviter->GetRange(), m_Indices, m_Indices + m_Len, resData.begin()+m_Start);
	}
	#define INSTANTIATE(E) \
		void Visit(const Unit<E>* inviter) const override { VisitImpl(inviter); }
		INSTANTIATE_CNT_ELEM
	#undef INSTANTIATE

	AbstrDataItem*   m_ResItem;
	AbstrDataObject* m_Res;
	tile_id          m_TileID;
//	WritableTileLock m_resTileLock;
	DataArrayBase<IndexType>::locked_seq_t m_IndexHolder;
	IndexType*       m_Indices;
	SizeT            m_Start, m_Len;

	std::unique_ptr<IndexType[]> m_ResBuffer;
};

using IndexAssigner32    = IndexAssignerBase<UInt32>;
using IndexAssignerSizeT = IndexAssignerBase<SizeT>;

/******************************************************************************/

struct IdAssigner : UnitProcessor
{
	IdAssigner(AbstrDataObject* res, tile_id t, SizeT start, SizeT base, SizeT len)
		:	m_Res(res)
		,	m_TileID(t)
		,	m_Start(start)
		,	m_Base(base)
		,	m_Len(len)
	{}

	template <typename E>
	void VisitImpl(const Unit<E>* inviter) const
	{
		auto resData = composite_cast<DataArray<E>*>(m_Res)->GetDataWrite(m_TileID, dms_rw_mode::read_write);
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
	#define INSTANTIATE(E) \
		void Visit(const Unit<E>* inviter) const override { VisitImpl(inviter); }
		INSTANTIATE_CNT_ELEM
	#undef INSTANTIATE

	AbstrDataObject* m_Res;
	tile_id        m_TileID;
	SizeT          m_Start, m_Base, m_Len;
};


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
		auto resData = composite_cast<DataArray<E>*>(m_Res)->GetDataWrite(m_TileID, dms_rw_mode::read_write);
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

struct NullAssigner : boost::mpl::fold<typelists::domain_elements, NullAssignerBase, VisitorImpl<Unit<_2>, _1> >::type
{
	using base_type = boost::mpl::fold<typelists::domain_elements, NullAssignerBase, VisitorImpl<Unit<_2>, _1> >::type;
	using base_type::base_type; // inherit ctors
};

/******************************************************************************/

#endif // !defined(__CLC_INDEXASSIGNER_H)

