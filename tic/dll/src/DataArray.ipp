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

#include "DataArray.h"
#include "ParallelTiles.h"

#include "geo/Conversions.h"

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


template <class V> 
void DataArrayBase<V>::SetIndexedValueArray(SizeT start, SizeT len, const api_t* srcData)
{
	locked_seq_t data = GetLockedDataWrite(); // NOT TILE AWARE
	SizeT end = start + len;
	if (data.size() < start || data.size() < end || end < start)
		throwErrorD("DataArrayBase", "SetIndexedValueArray: index out of range");

	iterator pi = data.begin() + start;
	iterator pe = data.begin() + end;
	while (pi != pe)
		Assign(*pi++, value_type(*srcData++) );
}


template <class V> 
void DataArrayBase<V>::GetIndexedValueArray(SizeT start, SizeT len, api_t* destination) const
{
	locked_cseq_t data = GetLockedDataRead(); // NOT TILE AWARE

	UInt32 end = start + len;
	if (data.size() < start || data.size() < end || end < start)
		throwErrorD("DataArrayBase", "GetIndexedValueArray: index out of range");

	const_iterator pi = data.begin() + start;
	const_iterator pe = data.begin() + end;
	while (pi != pe)
		*destination++ = value_type(*pi++);
}

// Overridden DataItem Get Functions

template <class V>
typename DataArrayBase<V>::value_type
DataArrayBase<V>::GetIndexedValue(SizeT index) const
{
	tile_loc tl = GetTiledLocation(index);
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
	tile_loc loc = GetTiledRangeData()->GetTiledLocation(index);
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
	Concurrency::combinable<SizeT> counts;
	
	parallel_tileloop(GetTiledRangeData()->GetNrTiles(), [v, this, &counts](tile_id t)->void
	{
		auto data = GetTile(t);
		counts.local() += std::count(data.begin(), data.end(), v);
	});
	return counts.combine(std::plus<SizeT>());
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
SizeT NumericArray<V>::FindPos(V v, SizeT startPos) const
{
	auto tn = this->GetTiledRangeData()->GetNrTiles();
	if (tn)
	{
		auto loc = this->GetTiledRangeData()->GetTiledLocation(startPos);
		for (tile_id tn = this->GetTiledRangeData()->GetNrTiles(); loc.first != tn; ++loc.first, loc.second = 0)
		{
			auto pos = vector_find(this->GetDataRead(loc.first), v, loc.second);
			if (IsDefined(pos))
				return this->GetTiledRangeData()->GetRowIndex(loc.first, pos);
		}
	}
	return UNDEFINED_VALUE(SizeT);
}

template <class V>
Float64 NumericArray<V>::GetValueAsFloat64(SizeT index) const
{
	CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);
	return conv.GetScalar<Float64>(this->GetIndexedValue(index));
}

template <class V>
void NumericArray<V>::SetValueAsFloat64(SizeT index, Float64 val)
{
	CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);
	this->SetIndexedValue(index, conv.GetValue(val));
}

template <class V>
SizeT NumericArray<V>::FindPosOfFloat64(Float64 val, SizeT startPos) const
{
	CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);
	return FindPos( conv.GetValue(val), startPos);
}

template <class V>
Int32 NumericArray<V>::GetValueAsInt32(SizeT index) const
{
	CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);
	return conv.GetScalar<Int32>(this->GetIndexedValue(index));
}

template <class V>
void NumericArray<V>::SetValueAsInt32(SizeT index, Int32 val)
{
	CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);
	this->SetIndexedValue(index, conv.GetValue(val) );
}

template <class V>
UInt32 NumericArray<V>::GetValueAsUInt32(SizeT index) const
{
	CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);
	return conv.GetScalar<UInt32>(this->GetIndexedValue(index));
}

template <class V>
SizeT NumericArray<V>::GetValueAsSizeT(SizeT index) const
{
	CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);
	return conv.GetScalar<SizeT>(this->GetIndexedValue(index));
}

template <class V>
void NumericArray<V>::SetValueAsSizeT(SizeT index, SizeT val)
{
	CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);
	this->SetIndexedValue(index, conv.GetValue(val) );
}

template <class V>
void NumericArray<V>::SetValueAsSizeT(SizeT index, SizeT val, tile_id t)
{
	CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);
	this->SetTileIndexedValue(t, index, conv.GetValue(val));
}

template <class V>
UInt8 NumericArray<V>::GetValueAsUInt8(SizeT index) const
{
	CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);
	return conv.GetScalar<UInt8>(this->GetIndexedValue(index));
}

template <class V>
void NumericArray<V>::SetValueAsUInt32(SizeT index, UInt32 val)
{
	CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);
	this->SetIndexedValue(index, conv.GetValue(val) );
}

template <class V>
SizeT NumericArray<V>::FindPosOfSizeT(SizeT val, SizeT startPos) const
{
	CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);
	return FindPos( conv.GetValue(val), startPos );
}

//----------------------------------------------------------------------
// class  : AdditiveArray
//----------------------------------------------------------------------

template <class V>
Float64 AdditiveArray<V>::GetSumAsFloat64() const
{
	Float64 result = 0;
	auto data = this->GetLockedDataRead();
	
	for (auto i = begin_ptr(data), e = end_ptr(data); i!=e; ++i)
		if (IsDefined(*i))
			result += V(*i);
	return result;
}

//----------------------------------------------------------------------
// Support for numeric arrays
//----------------------------------------------------------------------

template <class V>
SizeT NumericArray<V>::GetValuesAsFloat64Array(tile_loc tl, SizeT len, Float64* data) const
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
		*data = conv.GetScalar<Float64>(*pi);
	return len;
}

template <class V>
SizeT NumericArray<V>::GetValuesAsUInt32Array  (tile_loc tl, SizeT len, UInt32* data) const
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
		*data = conv.GetScalar<UInt32>(*pi);
	return len;
}

template <class V>
SizeT NumericArray<V>::GetValuesAsInt32Array(tile_loc tl, SizeT len, Int32* data) const
{
	CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);

	auto elemData = this->GetDataRead(tl.first);
	if (elemData.size() < tl.second)
		throwErrorD("NumericArray<V>", "GetValuesAsInt32Array: index out of range");
	MakeMin(len, elemData.size() - tl.second);
	auto
		pi = elemData.begin() + tl.second,
		pe = pi + len;
	for (; pi != pe; ++pi, ++data)
		*data = conv.GetScalar<Int32>(*pi);
	return len;
}

template <class V>
SizeT NumericArray<V>::GetValuesAsUInt8Array  (tile_loc tl, SizeT len, UInt8* data) const
{
	CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);

	auto elemData = this->GetDataRead(tl.first);
	if (elemData.size() < tl.second)
		throwErrorD("NumericArray<V>", "GetValuesAsUInt8Array: index out of range");
	MakeMin(len, elemData.size() - tl.second);
	auto
		pi = elemData.begin() + tl.second,
		pe = pi + len;
	for (; pi != pe; ++pi, ++data)
		*data = conv.GetScalar<UInt8>(*pi);
	return len;
}

template <class V>
void NumericArray<V>::SetValuesAsFloat64Array(tile_loc tl, SizeT len, const Float64* srcData)
{
	CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);

	auto data = this->GetDataWrite(tl.first, dms_rw_mode::read_write);

	dms_assert( tl.second <= tl.second + len && tl.second + len <= data.size() );

	auto pi = data.begin() + tl.second;
	auto pe = pi + len;
	for (; pi != pe; ++pi, ++srcData)
		Assign(*pi, conv.GetValue(*srcData) );
}

template <class V>
void NumericArray<V>::SetValuesAsInt32Array  (tile_loc tl, SizeT len, const Int32* srcData)
{
	CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);

	auto data = this->GetDataWrite(tl.first, dms_rw_mode::read_write);

	dms_assert( tl.second <= tl.second + len && tl.second + len <= data.size() );

	auto pi = data.begin() + tl.second;
	auto pe = pi + len;
	for (; pi != pe; ++pi, ++srcData)
		Assign(*pi, conv.GetValue(*srcData) );
}

template <class V>
void NumericArray<V>::SetValuesAsUInt8Array  (tile_loc tl, SizeT len, const UInt8* srcData)
{
	CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);

	auto data = this->GetDataWrite(tl.first, dms_rw_mode::read_write);

	dms_assert( tl.second <= tl.second + len && tl.second + len <= data.size() );

	auto pi = data.begin() + tl.second;
	auto pe = pi + len;
	for (; pi != pe; ++pi, ++srcData)
		Assign(*pi, conv.GetValue(*srcData) );
}

template <class V>
void NumericArray<V>::FillWithFloat64Values  (tile_loc tl, SizeT len, Float64 fillValue)
{
	CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);

	auto data = this->GetDataWrite(tl.first, dms_rw_mode::read_write);

	dms_assert( tl.second <= tl.second + len && tl.second + len <= data.size() );

	auto pi = data.begin() + tl.second;

	fast_fill(pi, pi + len, conv.GetValue(fillValue));
}

template <class V>
void NumericArray<V>::FillWithUInt32Values  (tile_loc tl, SizeT len, UInt32 fillValue)
{
	CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);

	auto data = this->GetDataWrite(tl.first, dms_rw_mode::read_write);

	dms_assert( tl.second <= tl.second + len && tl.second + len <= data.size() );

	auto pi = data.begin() + tl.second;

	fast_fill(pi, pi + len, conv.GetValue(fillValue));
}

template <class V>
void NumericArray<V>::FillWithInt32Values  (tile_loc tl, SizeT len, Int32 fillValue)
{
	CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);

	auto data = this->GetDataWrite(tl.first, dms_rw_mode::read_write);

	dms_assert( tl.second <= tl.second + len && tl.second + len <= data.size() );

	auto pi = data.begin() + tl.second;

	fast_fill(pi, pi + len, conv.GetValue(fillValue));
}

template <class V>
void NumericArray<V>::FillWithUInt8Values  (tile_loc tl, SizeT len, UInt8 fillValue)
{
	CountablePointConverter<V> conv(this->m_ValueRangeDataPtr);

	auto data = this->GetDataWrite(tl.first, dms_rw_mode::read_write);

	dms_assert( tl.second <= tl.second + len && tl.second + len <= data.size() );

	auto pi = data.begin() + tl.second;
	
	fast_fill(pi, pi + len, conv.GetValue(fillValue));
}

//----------------------------------------------------------------------
// Support for Geometrics
//----------------------------------------------------------------------

template <typename Base>
DRect GeoArrayAdapter<Base>::GetActualRangeAsDRect(bool checkForNulls) const
{
	using field_t = typename Base::field_t;
	Range<field_t> result;
	for (tile_id t = 0, tn = this->GetTiledRangeData()->GetNrTiles(); t != tn; ++t)
	{
		auto data = this->GetDataRead(t);

		result |=
			Range<field_t>(
				data.begin(), 
				data.end(), 
				checkForNulls,
				false // don't call MakeStrictlyGreater on upper bound of the range
			);
	}
	return Convert<DRect>(result);
}

//----------------------------------------------------------------------
// Support for GeometricPoints
//----------------------------------------------------------------------

template <typename Base>
DPoint  PointArrayAdapter<Base>::GetValueAsDPoint(SizeT index) const
{
	return Convert<DPoint>(this->GetIndexedValue(index));
}

template <typename Base>
void PointArrayAdapter<Base>::SetValueAsDPoint(SizeT index, const DPoint& val)
{
	this->SetIndexedValue(index, Convert<typename Base::value_type>(val) );
}

//----------------------------------------------------------------------
// Support for GeometricSequences
//----------------------------------------------------------------------

template <typename Base>
void SeqArrayAdapter<Base>::GetValueAsDPoints(SizeT index, std::vector<DPoint>& dpoints) const
{
	typename Base::value_type value = this->GetIndexedValue(index);
	dpoints.clear();
	dpoints.reserve(value.size());
	for (auto i = value.begin(), e=value.end(); i!=e; ++i)
		dpoints.push_back(Convert<DPoint>(*i));
}

