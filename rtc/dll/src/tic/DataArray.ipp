// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "DataArray.h"
#include "ParallelTiles.h"

#include "vt/Conversions.h"
#include "set/VectorFunc.h"

#include "Unit.h"

#include <numeric>

//----------------------------------------------------------------------
// class  : DataArrayBase memberfunc impl
//----------------------------------------------------------------------

template <class V> 
void DataArrayBase<V>::SetIndexedValue(SizeT index, param_t value)
{
	auto tl = GetTiledLocation(index);
	SetTileIndexedValue(tl.first, tl.second, value);
}

template <class V>
void DataArrayBase<V>::SetTileIndexedValue(tile_id t, tile_offset index, param_t value)
{
	dms_assert(t != no_tile);
	auto data = GetWritableTile(t, dms_rw_mode::read_write);
	Assign(data[index], value);
}


// Overridden DataItem Get Functions

template <class V>
typename DataArrayBase<V>::value_type
DataArrayBase<V>::GetIndexedValue(SizeT index) const
{
	tile_loc tl = GetTileDataLocation(index);
	if (!IsDefined(tl.first))
		return UNDEFINED_OR_ZERO(V);

	auto data = GetTile(tl.first);
	CheckIndexToTileDataSize(tl.second, data.size());
	return Convert<value_type>(data[tl.second]);
}

template <class V>
typename DataArrayBase<V>::const_iterator
DataArrayBase<V>::GetIndexedIterator(SizeT index, GuiReadLock& lockHolder) const
{
	tile_loc loc = GetTiledRangeData()->GetTileDataLocation(index);
	if (!IsDefined(loc.first))
		return const_iterator();

	auto tilePtr = GetTile(loc.first);
	lockHolder = tilePtr.m_TileHolder;
	CheckIndexToTileDataSize(loc.second, tilePtr.size());
	return tilePtr.begin() + loc.second;
}

template <class V>
SizeT DataArrayBase<V>::CountValues(param_t v) const
{
	std::atomic<SizeT> count{0};

	parallel_tileloop(this->GetTiledRangeData()->GetNrTiles(), [v, this, &count](tile_id t)->void
	{
		auto data = this->GetTile(t);
		count += std::count(data.begin(), data.end(), v);
	});
	return count.load();
}

// override AbstrDataItem
row_id AbstrDataObject::GetNrFeaturesNow() const
{
	return GetTiledRangeData()->GetElemCount();
}

// override AbstrDataItem
std::size_t AbstrDataObject::GetNrBytesNow(bool calcStreamSize) const
{
	SizeT x=0;
	for (tile_id u = 0, e = GetTiledRangeData()->GetNrTiles(); u != e; ++u)
		x += GetNrTileBytesNow(u, calcStreamSize);
	return x;
}

template <class V>
std::size_t DataArrayBase<V>::GetNrTileBytesNow(tile_id t, bool calcStreamSize) const
{
	dms_assert(t < GetTiledRangeData()->GetNrTiles());
	auto tile = GetTile(t);
	if (tile.size() == 0)
		return std::size_t(-1); // can be anything
	return NrBytesOf(tile, calcStreamSize);
}

template <class V>
bool DataArrayBase<V>::IsSmallerThan(SizeT sz) const
{
	auto nrTiles = GetTiledRangeData()->GetNrTiles();
	if (nrTiles != 1)
		return nrTiles == 0; // more than one tile is always bigger than sz.

	return GetNrTileBytesNow(0, false) < sz;

}

//----------------------------------------------------------------------
// Support for numerics
//----------------------------------------------------------------------

template <class V>
SizeT DataArrayBase<V>::FindPos(V v, SizeT startPos) const requires numeric_array_api_v<V>
{
	auto tn = this->GetTiledRangeData()->GetNrTiles();
	if (tn)
	{
		auto loc = this->GetTiledRangeData()->GetTiledLocation(startPos);
		if (!IsDefined(loc.first))
		{
			if (!startPos)
				loc = { 0, 0 }; // Irregular tile start
			else
			{
				// Irregular tile continuation after last element of tile:
				// go back, pick up location, go forward locally and go.
				loc = this->GetTiledRangeData()->GetTiledLocation(startPos - 1); 
				MG_CHECK(loc.first < tn);
				++loc.first;
				loc.second = 0;
			}
		}
		for (; loc.first < tn; ++loc.first, loc.second = 0)
		{
			auto pos = vector_find(this->GetTile(loc.first), v, loc.second);
			if (IsDefined(pos))
				return this->GetTiledRangeData()->GetRowIndex(loc.first, pos);
		}
	}
	return UNDEFINED_VALUE(SizeT);
}

template <class V>
Float64 DataArrayBase<V>::GetValueAsFloat64(SizeT index) const
{
	if constexpr (numeric_array_api_v<V>)
	{
		CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);
		return conv.template GetScalar<Float64>(this->GetIndexedValue(index));
	}
	else
		return AbstrDataObject::GetValueAsFloat64(index);
}

template <class V>
void DataArrayBase<V>::SetValueAsFloat64(SizeT index, Float64 val)
{
	if constexpr (numeric_array_api_v<V>)
	{
		CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);
		this->SetIndexedValue(index, conv.GetValue(val));
	}
	else
		AbstrDataObject::SetValueAsFloat64(index, val);
}

template <class V>
SizeT DataArrayBase<V>::FindPosOfFloat64(Float64 val, SizeT startPos) const
{
	if constexpr (numeric_array_api_v<V>)
	{
		CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);
		return FindPos( conv.GetValue(val), startPos);
	}
	else
		return AbstrDataObject::FindPosOfFloat64(val, startPos);
}

template <class V>
Int32 DataArrayBase<V>::GetValueAsInt32(SizeT index) const
{
	if constexpr (numeric_array_api_v<V>)
	{
		CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);
		return conv.template GetScalar<Int32>(this->GetIndexedValue(index));
	}
	else
		return AbstrDataObject::GetValueAsInt32(index);
}

template <class V>
void DataArrayBase<V>::SetValueAsInt32(SizeT index, Int32 val)
{
	if constexpr (numeric_array_api_v<V>)
	{
		CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);
		this->SetIndexedValue(index, conv.GetValue(val) );
	}
	else
		AbstrDataObject::SetValueAsInt32(index, val);
}

template <class V>
UInt32 DataArrayBase<V>::GetValueAsUInt32(SizeT index) const
{
	if constexpr (numeric_array_api_v<V>)
	{
		CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);
		return conv.template GetScalar<UInt32>(this->GetIndexedValue(index));
	}
	else
		return AbstrDataObject::GetValueAsUInt32(index);
}

template <class V>
SizeT DataArrayBase<V>::GetValueAsSizeT(SizeT index) const
{
	if constexpr (numeric_array_api_v<V>)
	{
		CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);
		return conv.template GetScalar<SizeT>(this->GetIndexedValue(index));
	}
	else
		return AbstrDataObject::GetValueAsSizeT(index);
}

template <class V>
void DataArrayBase<V>::SetValueAsSizeT(SizeT index, SizeT val)
{
	if constexpr (numeric_array_api_v<V>)
	{
		CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);
		this->SetIndexedValue(index, conv.GetValue(val) );
	}
	else
		AbstrDataObject::SetValueAsSizeT(index, val);
}

template <class V>
void DataArrayBase<V>::SetValueAsDiffT(SizeT index, DiffT val)
{
	if constexpr (numeric_array_api_v<V>)
	{
		CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);
		this->SetIndexedValue(index, conv.GetValue(val));
	}
	else
		AbstrDataObject::SetValueAsDiffT(index, val);
}

template <class V>
void DataArrayBase<V>::SetValueAsSizeT(SizeT index, SizeT val, tile_id t)
{
	if constexpr (numeric_array_api_v<V>)
	{
		CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);
		this->SetTileIndexedValue(t, index, conv.GetValue(val));
	}
	else
		AbstrDataObject::SetValueAsSizeT(index, val, t);
}

template <class V>
UInt8 DataArrayBase<V>::GetValueAsUInt8(SizeT index) const
{
	if constexpr (numeric_array_api_v<V>)
	{
		CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);
		return conv.template GetScalar<UInt8>(this->GetIndexedValue(index));
	}
	else
		return AbstrDataObject::GetValueAsUInt8(index);
}

template <class V>
void DataArrayBase<V>::SetValueAsUInt32(SizeT index, UInt32 val)
{
	if constexpr (numeric_array_api_v<V>)
	{
		CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);
		this->SetIndexedValue(index, conv.GetValue(val) );
	}
	else
		AbstrDataObject::SetValueAsUInt32(index, val);
}

template <class V>
SizeT DataArrayBase<V>::FindPosOfSizeT(SizeT val, SizeT startPos) const
{
	if constexpr (numeric_array_api_v<V>)
	{
		CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);
		return FindPos( conv.GetValue(val), startPos );
	}
	else
		return AbstrDataObject::FindPosOfSizeT(val, startPos);
}

//----------------------------------------------------------------------
// Support for additive numerics: the scalar value types only (was AdditiveArray)
//----------------------------------------------------------------------

template <class V>
Float64 DataArrayBase<V>::GetSumAsFloat64() const
{
	if constexpr (numeric_elem_v<V>)
	{
		Float64 result = 0;
		auto data = this->GetLockedDataRead();

		for (auto i = begin_ptr(data), e = end_ptr(data); i!=e; ++i)
			if (IsDefined(*i))
				result += V(*i);
		return result;
	}
	else
		return AbstrDataObject::GetSumAsFloat64();
}

//----------------------------------------------------------------------
// Support for numeric arrays
//----------------------------------------------------------------------

template <class V>
SizeT DataArrayBase<V>::GetValuesAsFloat64Array(tile_loc tl, SizeT len, Float64* data) const
{
	if constexpr (numeric_array_api_v<V>)
	{
		CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);

		auto elemData = this->GetDataRead(tl.first);
		if (elemData.size() < tl.second)
			throwDmsErrD("DataArrayBase::GetValuesAsFloat64Array: index out of range");
		MakeMin(len, elemData.size() - tl.second);
		auto
			pi = elemData.begin() + tl.second,
			pe = pi + len;
		for (; pi != pe; ++pi, ++data)
			*data = conv.template GetScalar<Float64>(*pi);
		return len;
	}
	else
		return AbstrDataObject::GetValuesAsFloat64Array(tl, len, data);
}

template <class V>
SizeT DataArrayBase<V>::GetValuesAsUInt32Array  (tile_loc tl, SizeT len, UInt32* data) const
{
	if constexpr (numeric_array_api_v<V>)
	{
		CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);

		auto elemData = this->GetDataRead(tl.first);
		if (elemData.size() < tl.second)
			throwDmsErrD("DataArrayBase::GetValuesAsUInt32Array: index out of range");
		MakeMin(len, elemData.size() - tl.second);
		auto
			pi = elemData.begin() + tl.second,
			pe = pi + len;
		for (; pi != pe; ++pi, ++data)
			*data = conv.template GetScalar<UInt32>(*pi);
		return len;
	}
	else
		return AbstrDataObject::GetValuesAsUInt32Array(tl, len, data);
}

template <class V>
SizeT DataArrayBase<V>::GetValuesAsInt32Array(tile_loc tl, SizeT len, Int32* data) const
{
	if constexpr (numeric_array_api_v<V>)
	{
		CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);

		auto elemData = this->GetDataRead(tl.first);
		if (elemData.size() < tl.second)
			throwErrorD("DataArrayBase<V>", "GetValuesAsInt32Array: index out of range");
		MakeMin(len, elemData.size() - tl.second);
		auto
			pi = elemData.begin() + tl.second,
			pe = pi + len;
		for (; pi != pe; ++pi, ++data)
			*data = conv.template GetScalar<Int32>(*pi);
		return len;
	}
	else
		return AbstrDataObject::GetValuesAsInt32Array(tl, len, data);
}

template <class V>
SizeT DataArrayBase<V>::GetValuesAsUInt8Array  (tile_loc tl, SizeT len, UInt8* data) const
{
	if constexpr (numeric_array_api_v<V>)
	{
		CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);

		auto elemData = this->GetDataRead(tl.first);
		if (elemData.size() < tl.second)
			throwErrorD("DataArrayBase<V>", "GetValuesAsUInt8Array: index out of range");
		MakeMin(len, elemData.size() - tl.second);
		auto
			pi = elemData.begin() + tl.second,
			pe = pi + len;
		for (; pi != pe; ++pi, ++data)
			*data = conv.template GetScalar<UInt8>(*pi);
		return len;
	}
	else
		return AbstrDataObject::GetValuesAsUInt8Array(tl, len, data);
}

template <class V>
void DataArrayBase<V>::SetValuesAsFloat64Array(tile_loc tl, SizeT len, const Float64* srcData)
{
	if constexpr (numeric_array_api_v<V>)
	{
		CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);

		auto data = this->GetDataWrite(tl.first, dms_rw_mode::read_write);

		assert( tl.second <= tl.second + len && tl.second + len <= data.size() );

		auto pi = data.begin() + tl.second;
		auto pe = pi + len;
		for (; pi != pe; ++pi, ++srcData)
			Assign(*pi, conv.GetValue(*srcData) );
	}
	else
		AbstrDataObject::SetValuesAsFloat64Array(tl, len, srcData);
}

template <class V>
void DataArrayBase<V>::SetValuesAsInt32Array  (tile_loc tl, SizeT len, const Int32* srcData)
{
	if constexpr (numeric_array_api_v<V>)
	{
		CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);

		auto data = this->GetDataWrite(tl.first, dms_rw_mode::read_write);

		dms_assert( tl.second <= tl.second + len && tl.second + len <= data.size() );

		auto pi = data.begin() + tl.second;
		auto pe = pi + len;
		for (; pi != pe; ++pi, ++srcData)
			Assign(*pi, conv.GetValue(*srcData) );
	}
	else
		AbstrDataObject::SetValuesAsInt32Array(tl, len, srcData);
}

template <class V>
void DataArrayBase<V>::SetValuesAsUInt8Array  (tile_loc tl, SizeT len, const UInt8* srcData)
{
	if constexpr (numeric_array_api_v<V>)
	{
		CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);

		auto data = this->GetDataWrite(tl.first, dms_rw_mode::read_write);

		dms_assert( tl.second <= tl.second + len && tl.second + len <= data.size() );

		auto pi = data.begin() + tl.second;
		auto pe = pi + len;
		for (; pi != pe; ++pi, ++srcData)
			Assign(*pi, conv.GetValue(*srcData) );
	}
	else
		AbstrDataObject::SetValuesAsUInt8Array(tl, len, srcData);
}

template <class V>
void DataArrayBase<V>::FillWithFloat64Values  (tile_loc tl, SizeT len, Float64 fillValue)
{
	if constexpr (numeric_array_api_v<V>)
	{
		CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);

		auto data = this->GetDataWrite(tl.first, dms_rw_mode::read_write);

		dms_assert( tl.second <= tl.second + len && tl.second + len <= data.size() );

		auto pi = data.begin() + tl.second;

		fast_fill(pi, pi + len, conv.GetValue(fillValue));
	}
	else
		AbstrDataObject::FillWithFloat64Values(tl, len, fillValue);
}

template <class V>
void DataArrayBase<V>::FillWithUInt32Values  (tile_loc tl, SizeT len, UInt32 fillValue)
{
	if constexpr (numeric_array_api_v<V>)
	{
		CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);

		auto data = this->GetDataWrite(tl.first, dms_rw_mode::read_write);

		dms_assert( tl.second <= tl.second + len && tl.second + len <= data.size() );

		auto pi = data.begin() + tl.second;

		fast_fill(pi, pi + len, conv.GetValue(fillValue));
	}
	else
		AbstrDataObject::FillWithUInt32Values(tl, len, fillValue);
}

template <class V>
void DataArrayBase<V>::FillWithInt32Values  (tile_loc tl, SizeT len, Int32 fillValue)
{
	if constexpr (numeric_array_api_v<V>)
	{
		CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);

		auto data = this->GetDataWrite(tl.first, dms_rw_mode::read_write);

		dms_assert( tl.second <= tl.second + len && tl.second + len <= data.size() );

		auto pi = data.begin() + tl.second;

		fast_fill(pi, pi + len, conv.GetValue(fillValue));
	}
	else
		AbstrDataObject::FillWithInt32Values(tl, len, fillValue);
}

template <class V>
void DataArrayBase<V>::FillWithUInt8Values  (tile_loc tl, SizeT len, UInt8 fillValue)
{
	if constexpr (numeric_array_api_v<V>)
	{
		CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);

		auto data = this->GetDataWrite(tl.first, dms_rw_mode::read_write);

		dms_assert( tl.second <= tl.second + len && tl.second + len <= data.size() );

		auto pi = data.begin() + tl.second;

		fast_fill(pi, pi + len, conv.GetValue(fillValue));
	}
	else
		AbstrDataObject::FillWithUInt8Values(tl, len, fillValue);
}

//----------------------------------------------------------------------
// Support for Geometrics
//----------------------------------------------------------------------

template <typename V>
DRect DataArrayBase<V>::GetActualRangeAsDRect(bool checkForNulls) const
{
	if constexpr (geo_elem_v<V>)
	{
		Range<field_t> result;
		std::mutex resultMutationCS;
		parallel_tileloop(this->GetTiledRangeData()->GetNrTiles(), [&result, &resultMutationCS, this, checkForNulls](tile_id t)
			{
				auto data = this->GetTile(t);
				auto range =
					Range<field_t>(data.begin(), data.end()
					,	checkForNulls
					,	false // don't call MakeStrictlyGreater on upper bound of the range
					);
				std::lock_guard exclusiveAccessLock(resultMutationCS);
				result |= range;
			}
		);
		return Convert<DRect>(result);
	}
	else
		return AbstrDataObject::GetActualRangeAsDRect(checkForNulls);
}

//----------------------------------------------------------------------
// Support for GeometricPoints: the point types only (was PointArrayAdapter)
//----------------------------------------------------------------------

template <typename V>
DPoint  DataArrayBase<V>::GetValueAsDPoint(SizeT index) const
{
	if constexpr (point_elem_v<V>)
		return Convert<DPoint>(this->GetIndexedValue(index));
	else
		return AbstrDataObject::GetValueAsDPoint(index);
}

template <typename V>
void DataArrayBase<V>::SetValueAsDPoint(SizeT index, const DPoint& val)
{
	if constexpr (point_elem_v<V>)
		this->SetIndexedValue(index, Convert<value_type>(val) );
	else
		AbstrDataObject::SetValueAsDPoint(index, val);
}

//----------------------------------------------------------------------
// Support for GeometricSequences: the point polygons only (was SeqArrayAdapter)
//----------------------------------------------------------------------

template <typename V>
void DataArrayBase<V>::GetValueAsDPoints(SizeT index, std::vector<DPoint>& dpoints) const
{
	if constexpr (polygon_elem_v<V>)
	{
		value_type value = this->GetIndexedValue(index);
		dpoints.clear();
		dpoints.reserve(value.size());
		for (auto i = value.begin(), e=value.end(); i!=e; ++i)
			dpoints.push_back(Convert<DPoint>(*i));
	}
	else
		AbstrDataObject::GetValueAsDPoints(index, dpoints);
}

