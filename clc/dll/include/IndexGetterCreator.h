// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(DMS_CLC_INDEXGETTER_H)
#define DMS_CLC_INDEXGETTER_H

#include "AbstrDataObject.h"
#include "ValueGetter.h"

//=================================== IndexGetter

using IndexGetter = AbstrValueGetter<SizeT> ;

struct IndexGetterCreatorBase : UnitProcessor
{
	template <typename E>
	CLC_CALL void VisitImpl(const Unit<E>* inviter) const;

	template <int N>
	CLC_CALL void VisitImpl(const Unit<bit_value<N>>* inviter) const;

	WeakPtr<const AbstrDataItem> m_Adi;
	tile_id                      m_TileID = no_tile;
	abstr_future_tile* m_Aft = nullptr;
	mutable WeakPtr<IndexGetter> m_Result;
};

struct IndexGetterCreator :  boost::mpl::fold<typelists::domain_elements, IndexGetterCreatorBase, VisitorImpl<Unit<_2>, _1> >::type
{
	static CLC_CALL IndexGetter* Create(const AbstrDataItem* adi, tile_id t);
	static CLC_CALL IndexGetter* Create(const AbstrDataItem* adi, abstr_future_tile* aft);

private:
	IndexGetterCreator(const AbstrDataItem* adi, tile_id t);
	IndexGetterCreator(const AbstrDataItem* adi, abstr_future_tile* aft);
	IndexGetter* Create();
};


#endif //!defined(DMS_CLC_INDEXGETTER_H)
