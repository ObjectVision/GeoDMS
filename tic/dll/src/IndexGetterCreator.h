// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(DMS_TIC_INDEXGETTERCREATOR_H)
#define DMS_TIC_INDEXGETTERCREATOR_H

#include "TicBase.h"

#include "AbstrDataObject.h"
#include "DataCheckMode.h"
#include "ValueGetter.h"

#include "FutureTileArray.h"

//=================================== IndexGetter

using IndexGetter = AbstrValueGetter<SizeT> ;

struct adi_tile_getter
{
	SharedPtr<const AbstrDataItem> m_Adi;
	tile_id                        m_TileID = no_tile;

	template <typename E>
	auto get_tile() const -> typename DataArray<E>::locked_cseq_t
	{
		assert(m_Adi.get());
		const DataArray<E>* di = const_array_cast<E>(m_Adi);
		assert(di);
		return di->GetLockedDataRead(m_TileID);
	}
	auto get_checkmode() const
	{
		return m_Adi->GetCheckMode();
	}
};

struct aft_tile_getter
{
	abstr_future_tile_ptr m_Aft;
	DataCheckMode         m_CheckMode = DCM_CheckBoth;

	template <typename E>
	auto get_tile() const -> typename DataArray<E>::locked_cseq_t
	{
		using future_tile = typename DataArray<E>::future_tile;
		auto ft = dynamic_cast<future_tile*>(m_Aft.get());
		MG_CHECK(ft);
		return ft->GetTile();
	}
	auto get_checkmode() const
	{
		return m_CheckMode;
	};
};

template<typename TileCreationData>
struct IndexGetterCreatorBase : UnitProcessor
{
	template <typename E>
	TIC_CALL void VisitImpl(const Unit<E>* inviter) const;

	template <int N>
	TIC_CALL void VisitImpl(const Unit<bit_value<N>>* inviter) const;

	TileCreationData               m_TileCreationData;
	mutable WeakPtr<IndexGetter>   m_Result;
};

struct IndexGetterCreator1 : boost::mpl::fold<typelists::domain_elements, IndexGetterCreatorBase<adi_tile_getter>, VisitorImpl<Unit<_2>, _1> >::type
{
	IndexGetterCreator1(const AbstrDataItem* adi, tile_id t);
};

struct IndexGetterCreator2 : boost::mpl::fold<typelists::domain_elements, IndexGetterCreatorBase<aft_tile_getter>, VisitorImpl<Unit<_2>, _1> >::type
{
	IndexGetterCreator2(const AbstrDataItem* adi, abstr_future_tile_ptr aft);
};

struct IndexGetterCreator
{
	static TIC_CALL IndexGetter* Create(const AbstrDataItem* adi, tile_id t);
	static TIC_CALL IndexGetter* Create(const AbstrDataItem* adi, abstr_future_tile_ptr aft);
};


#endif //!defined(DMS_TIC_INDEXGETTERCREATOR_H)
