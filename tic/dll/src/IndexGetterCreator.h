// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(DMS_TIC_INDEXGETTERCREATOR_H)
#define DMS_TIC_INDEXGETTERCREATOR_H

#include "TicBase.h"

#include "AbstrDataObject.h"
#include "ValueGetter.h"

#include "FutureTileArray.h"

//=================================== IndexGetter

using IndexGetter = AbstrValueGetter<SizeT> ;

struct IndexGetterCreatorBase : UnitProcessor
{
	template <typename E>
	TIC_CALL void VisitImpl(const Unit<E>* inviter) const;

	template <int N>
	TIC_CALL void VisitImpl(const Unit<bit_value<N>>* inviter) const;

	SharedPtr<const AbstrDataItem> m_Adi;
	tile_id                        m_TileID = no_tile;
	abstr_future_tile_ptr          m_Aft;
	mutable WeakPtr<IndexGetter>   m_Result;
};

struct IndexGetterCreator :  boost::mpl::fold<typelists::domain_elements, IndexGetterCreatorBase, VisitorImpl<Unit<_2>, _1> >::type
{
	static TIC_CALL IndexGetter* Create(const AbstrDataItem* adi, tile_id t);
	static TIC_CALL IndexGetter* Create(const AbstrDataItem* adi, abstr_future_tile_ptr aft);

private:
	IndexGetterCreator(const AbstrDataItem* adi, tile_id t);
	IndexGetterCreator(const AbstrDataItem* adi, abstr_future_tile_ptr aft);
	IndexGetter* Create();
};


#endif //!defined(DMS_TIC_INDEXGETTERCREATOR_H)
