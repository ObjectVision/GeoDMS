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

#include "ClcPCH.h"
#pragma hdrstop

#include "IndexGetterCreator.h"

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

template <typename E> 
void IndexGetterCreatorBase::VisitImpl(const Unit<E>* inviter) const
{
	static_assert(has_undefines_v<E>);
	const DataArray<E>* di = const_array_cast<E>(m_Adi);
	auto range = di->GetValueRangeData()->GetRange();
	typename DataArray<E>::locked_cseq_t tileData;
	DataCheckMode dcmIndices = DCM_CheckBoth;
	if (m_Aft)
	{
		using future_tile = typename DataArray<E>::future_tile;
		auto ft = debug_cast<future_tile*>(m_Aft);
		tileData = ft->GetTile();
	}
	else
	{
		dcmIndices = m_Adi->GetCheckMode();
		tileData = di->GetLockedDataRead(m_TileID);
	}
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

template <int N>
void IndexGetterCreatorBase::VisitImpl(const Unit<bit_value<N>>* inviter) const
{
	static_assert( ! has_undefines_v<bit_value<N>>);

	m_Result = new ValueGetter<SizeT, bit_value<N>>(const_array_cast<bit_value<N>>(m_Adi), m_TileID);
}


IndexGetterCreator::IndexGetterCreator(const AbstrDataItem* adi, tile_id t)
{
	m_Adi       = adi;
	m_TileID    = t;
}

IndexGetterCreator::IndexGetterCreator(const AbstrDataItem* adi, abstr_future_tile* aft)
{
	m_Adi = adi;
	m_Aft = aft;
}

IndexGetter* IndexGetterCreator::Create()
{
	m_Adi->GetAbstrValuesUnit()->InviteUnitProcessor(*this);
	return m_Result.get_ptr();
}

IndexGetter* IndexGetterCreator::Create(const AbstrDataItem* adi, tile_id t)
{
	return IndexGetterCreator(adi, t).Create();
}

IndexGetter* IndexGetterCreator::Create(const AbstrDataItem* adi, abstr_future_tile* aft)
{
	return IndexGetterCreator(adi, aft).Create();
}


