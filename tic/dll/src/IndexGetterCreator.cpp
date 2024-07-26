// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "IndexGetterCreator.h"

#include "AbstrDataItem.h"
#include "DataArray.h"
#include "DataCheckMode.h"

//=================================== IndexGetter

template <typename V>
struct ZeroIndexGetter : IndexGetter
{
	ZeroIndexGetter(DataArray<V>::locked_cseq_t&& data, V upperBound)
		: m_Data(std::move(data))
		, m_UpperBound(upperBound)
	{}

	SizeT Get(SizeT i) const override
	{
		assert(i < m_Data.size());
		auto v = m_Data[i];
		assert(IsStrictlyLower(v, m_UpperBound));
		assert(IsDefined(v));

		return Range_GetIndex_naked_zbase(m_UpperBound, v);
	}

	typename DataArray<V>::locked_cseq_t m_Data;
	typename V                           m_UpperBound;
};

template <typename V>
struct NakedIndexGetter : IndexGetter
{
	NakedIndexGetter(DataArray<V>::locked_cseq_t&& data, typename Unit<V>::range_t range)
		: m_Data(std::move(data))
		, m_Range(range)
	{}

	SizeT Get(SizeT t) const override
	{
		assert(t < m_Data.size());
		assert(IsDefined(m_Data[t]));
		assert(IsIncluding(m_Range, m_Data[t]));

		return Range_GetIndex_naked(m_Range, m_Data[t]);
	}

	typename DataArray<V>::locked_cseq_t m_Data;
	typename Unit<V>::range_t            m_Range;
};

template <typename V>
struct BoundedIndexGetter : IndexGetter
{
	BoundedIndexGetter(DataArray<V>::locked_cseq_t&& data, V upperBound)
		: m_Data(std::move(data))
		, m_UpperBound(upperBound)
	{}

	SizeT Get(SizeT t) const override
	{
		assert(t < m_Data.size());
		typename unsigned_type<V>::type v = m_Data[t];
		if (!IsStrictlyLower(v, m_UpperBound))
			return UNDEFINED_VALUE(SizeT);
		assert(IsDefined(v));

		return Range_GetIndex_naked_zbase(m_UpperBound, v);
	}

	typename DataArray<V>::locked_cseq_t m_Data;
	typename unsigned_type<V>::type      m_UpperBound;
};

template <typename V>
struct RangedIndexGetter : IndexGetter
{
	RangedIndexGetter(DataArray<V>::locked_cseq_t&& data, typename Unit<V>::range_t range)
		: m_Data(std::move(data))
		, m_Range(range)
	{}

	SizeT Get(SizeT t) const override
	{
		assert(t < m_Data.size());
		typename DataArray<V>::const_reference v = m_Data[t];
		if (!IsIncluding(m_Range, v))
			return UNDEFINED_VALUE(SizeT);
		assert(IsDefined(v));
		return Range_GetIndex_naked(m_Range, v);
	}

	typename DataArray<V>::locked_cseq_t m_Data;
	typename Unit<V>::range_t            m_Range;
};

template <typename V>
struct ZeroNullIndexGetter : IndexGetter
{
	ZeroNullIndexGetter(DataArray<V>::locked_cseq_t&& data, V upperBound)
		: m_Data(std::move(data))
		, m_UpperBound(upperBound)
	{}

	SizeT Get(SizeT t) const override
	{
		assert(t < m_Data.size());
		typename DataArray<V>::const_reference v = m_Data[t];
		if (!IsDefined(v))
			return UNDEFINED_VALUE(SizeT);
		return Range_GetIndex_naked_zbase(m_UpperBound, v);
	}

	typename DataArray<V>::locked_cseq_t m_Data;
	V                                    m_UpperBound;
};

template <typename V>
struct NullIndexGetter : IndexGetter
{
	NullIndexGetter(DataArray<V>::locked_cseq_t&& data, typename Unit<V>::range_t range)
		: m_Data(std::move(data))
		, m_Range(range)
	{}

	SizeT Get(SizeT t) const override
	{
		assert(t < m_Data.size());
		typename DataArray<V>::const_reference v = m_Data[t];
		if (!IsDefined(v))
			return UNDEFINED_VALUE(SizeT);
		assert(IsIncluding(m_Range, m_Data[t]));
		return Range_GetIndex_naked(m_Range, v);
	}

	typename DataArray<V>::locked_cseq_t m_Data;
	typename Unit<V>::range_t            m_Range;
};

template <typename V>
struct CheckedIndexGetter : IndexGetter
{
	CheckedIndexGetter(DataArray<V>::locked_cseq_t&& data, typename Unit<V>::range_t range)
		: m_Data(std::move(data))
		, m_Range(range)
	{}

	SizeT Get(SizeT t) const override
	{
		assert(t < m_Data.size());
		return Range_GetIndex_checked(m_Range, m_Data[t]);
	}

	typename DataArray<V>::locked_cseq_t m_Data;
	typename Unit<V>::range_t            m_Range;
};

template <typename TileCreationData>
template <typename E>
void IndexGetterCreatorBase<TileCreationData>::VisitImpl(const Unit<E>* inviter) const
{
	static_assert(has_undefines_v<E>);
	auto range = inviter->GetRange();
	typename DataArray<E>::locked_cseq_t tileData = m_TileCreationData.template get_tile<E>();
	DataCheckMode dcmIndices = m_TileCreationData.get_checkmode();

	bool hasOutOfRangeIndices = dcmIndices & DCM_CheckRange;
	if (hasOutOfRangeIndices && !IsIncluding(range, UNDEFINED_VALUE(E)))
		reinterpret_cast<UInt32&>(dcmIndices) &= ~DCM_CheckDefined;

	if (!(dcmIndices & DCM_CheckDefined))
		if (!hasOutOfRangeIndices)
		{
			if (range.first == E())
				m_Result = new ZeroIndexGetter<E>(std::move(tileData), range.second);
			else
				m_Result = new NakedIndexGetter<E>( std::move(tileData), range );
		}
		else
			if (range.first == E())
			{
				MG_CHECK(IsLowerBound(E(), range.second));
				m_Result = new BoundedIndexGetter<E>( std::move(tileData),  range.second );
			}
			else
				m_Result = new RangedIndexGetter<E>(std::move(tileData), range);
	else
		if (!hasOutOfRangeIndices)
		{
			if (range.first == E())
				m_Result = new ZeroNullIndexGetter<E>( std::move(tileData),  range.second );
			else
				m_Result = new NullIndexGetter<E>( std::move(tileData),  range );
		}
		else
		{
			assert(IsIncluding(range, UNDEFINED_VALUE(E))); // GetTiledCheckMode would change to only range checking if null is outside the range.
			m_Result = new CheckedIndexGetter<E>( std::move(tileData),  range );
		}
}

template <typename TileCreationData>
template <int N>
void IndexGetterCreatorBase<TileCreationData>::VisitImpl(const Unit<bit_value<N>>* inviter) const
{
	static_assert( ! has_undefines_v<bit_value<N>>);

	auto tileData = m_TileCreationData.template get_tile<bit_value<N>>();
	m_Result = new ValueGetter<SizeT, bit_value<N>>(std::move(tileData));
}

IndexGetterCreator1::IndexGetterCreator1(const AbstrDataItem* adi, tile_id t)
{
	m_TileCreationData.m_Adi       = adi;
	m_TileCreationData.m_TileID    = t;
}

IndexGetterCreator2::IndexGetterCreator2(const AbstrDataItem* adi, abstr_future_tile_ptr aft)
{
	m_TileCreationData.m_Aft = aft;
	m_TileCreationData.m_CheckMode = adi->GetCheckMode();
}

IndexGetter* IndexGetterCreator::Create(const AbstrDataItem* adi, tile_id t)
{
	auto igc = IndexGetterCreator1(adi, t);
	adi->GetAbstrValuesUnit()->InviteUnitProcessor(igc);
	return igc.m_Result.get_ptr();
}

IndexGetter* IndexGetterCreator::Create(const AbstrDataItem* adi, abstr_future_tile_ptr aft)
{
	auto igc = IndexGetterCreator2(adi, aft);
	adi->GetAbstrValuesUnit()->InviteUnitProcessor(igc);
	return igc.m_Result.get_ptr();
}


