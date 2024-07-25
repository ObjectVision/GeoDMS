// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(DMS_TIC_VALUEGETTER_H)
#define DMS_TIC_VALUEGETTER_H

#include "UnitProcessor.h"

template <typename T>
struct AbstrValueGetter
{
	virtual ~AbstrValueGetter()  {}
	virtual T Get(SizeT t) const = 0;
	virtual bool AllDefined() const { return false; }
};

//=================================== ValueGetter

template <typename T, typename V>
struct ValueGetter : AbstrValueGetter<T>
{
	ValueGetter(const DataArray<V>* ado, tile_id t)
		:	m_Data(ado->GetDataRead(t) )
	{}
	ValueGetter(typename DataArray<V>::locked_cseq_t&& data)
		: m_Data(std::move(data))
	{}

	T Get(SizeT t) const override
	{
		dms_assert(t < m_Data.size());
		return Convert<T>(m_Data[t]);
	}

	typename DataArray<V>::locked_cseq_t m_Data;
};

template <typename T>
struct ValueGetterCreatorBase : UnitProcessor
{
	template <typename E>
	void VisitImpl(const Unit<E>* inviter) const
	{
		m_Result = new ValueGetter<T, E>(const_array_cast<E>(m_Adi), m_TileID);
	}
	WeakPtr<const AbstrDataItem>         m_Adi;
	tile_id                              m_TileID = no_tile;
	mutable WeakPtr<AbstrValueGetter<T>> m_Result;
};

template <typename T, typename TL>
struct ValueGetterCreator :  boost::mpl::fold<TL, ValueGetterCreatorBase<T>, VisitorImpl<Unit<_2>, _1> >::type
{
	ValueGetterCreator(const AbstrDataItem* adi, tile_id t)
	{
		this->m_Adi    = adi;
		this->m_TileID = t;
	}
	AbstrValueGetter<T>* Create()
	{
		this->m_Adi->GetAbstrValuesUnit()->InviteUnitProcessor(*this);
		return this->m_Result.get_ptr();
	}
	static AbstrValueGetter<T>* Create(const AbstrDataItem* adi, tile_id t = no_tile)
	{
		return ValueGetterCreator(adi, t).Create();
	}
};


using WeightGetter = AbstrValueGetter<Float64>;
using WeightGetterCreator = ValueGetterCreator<Float64, typelists::num_objects>;

#endif //!defined(DMS_TIC_VALUEGETTER_H)
