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
#pragma once

#if !defined(__TIC_DATAARRAY_H)
#define __TIC_DATAARRAY_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include <set/VectorFunc.h>
#include "geo/StringBounds.h"
#include "geo/GeoSequence.h"
#include "geo/IterRange.h"
#include "geo/SequenceArray.h"

#include "AbstrDataObject.h"
#include "TiledRangeData.h"
#include "TileLock.h"

//----------------------------------------------------------------------
// class  : DataArrayBase
//----------------------------------------------------------------------

template <typename V>
struct DataArrayBase : AbstrDataObject
{
	using base_type = AbstrDataObject;

	typedef typename sequence_traits<V>::value_type              value_type;
	typedef typename param_type<value_type>::type                param_t;
	typedef typename api_type<value_type>::type                  api_t;
	typedef typename sequence_traits<value_type>:: seq_t         seq_t_HIDE;
	typedef typename sequence_traits<value_type>::cseq_t         cseq_t_HIDE;
	typedef typename locked_seq< seq_t_HIDE, TileRef>            locked_seq_t;
	typedef typename locked_seq<cseq_t_HIDE, TileCRef>           locked_cseq_t;

	typedef typename sequence_traits<value_type>::container_type container_t;

	typedef typename container_t::reference                      reference;
	typedef typename container_t::const_reference                const_reference;

	typedef typename seq_t_HIDE::iterator                        iterator;
	typedef typename cseq_t_HIDE::const_iterator                 const_iterator;

	typedef typename field_of<V>::type                           field_t;
	typedef Unit<field_t>                                        unit_t;

	using value_range_ptr_t = range_data_ptr_or_void<field_of_t<V>>;

	TICTOC_CALL AbstrReadableTileData* CreateReadableTileData(tile_id t) const override;

	TIC_CALL locked_cseq_t GetDataRead(tile_id t = no_tile) const;
	TIC_CALL locked_seq_t GetDataWrite(tile_id t = no_tile, dms_rw_mode rwMode = dms_rw_mode::read_write);

	auto GetReadableTileLock(tile_id t) const->TileCRef override { return GetTile(t).m_TileHolder; } // TODO G8: REMOVE
	auto GetWritableTileLock(tile_id t, dms_rw_mode rwMode = dms_rw_mode::read_write) ->TileRef override { return GetWritableTile(t, rwMode).m_TileHolder; } // TODO G8: REMOVE

	auto GetLockedDataRead(tile_id t = no_tile) const { return GetDataRead(t); } // TODO G8: SUBSTITUTE AWAY
	auto GetLockedDataWrite(tile_id t = no_tile, dms_rw_mode rwMode = dms_rw_mode::read_write) { return GetDataWrite(t, rwMode); } // TODO G8: SUBSTITUTE AWAY

	TIC_CALL SizeT CountValues(param_t v) const;

//	set data functions
	TIC_CALL void SetIndexedValue(SizeT index, param_t value);
	TIC_CALL void SetTileIndexedValue(tile_id t, tile_offset index, param_t value);
	TIC_CALL void SetIndexedValueArray(SizeT start, SizeT len, const api_t* pClientBuffer);

//	get data functions
	TIC_CALL value_type     GetIndexedValue   (SizeT index)    const;
	TIC_CALL const_iterator GetIndexedIterator(SizeT index, GuiReadLock& lockHolder)    const;
	TIC_CALL void GetIndexedValueArray (SizeT start, SizeT len, api_t* pClientBuffer) const;

//	override AbstrDataObject
	TICTOC_CALL bool CheckValuesUnit(const AbstrUnit* valuesUnit) override;

	//	Data Access
	TICTOC_CALL auto GetDataReadBegin(tile_id = no_tile) const->data_read_begin_handle override;
	TICTOC_CALL auto GetDataWriteBegin(tile_id = no_tile)->data_write_begin_handle override;

	TICTOC_CALL const ValueClass* GetValueClass() const override;
	TICTOC_CALL AbstrValue* CreateAbstrValue()                                     const override;
	TICTOC_CALL void GetAbstrValue(SizeT index, AbstrValue& valueHolder)    const override;
	TICTOC_CALL void SetAbstrValue(SizeT index, const AbstrValue& valueHolder)   override;
	TICTOC_CALL void        SetNull      (SizeT index)                                  override;
	TICTOC_CALL bool        IsNull       (SizeT index) const                            override;
	TICTOC_CALL SizeT       GetNrNulls   () const                                       override;

	TICTOC_CALL bool        AsCharArray(SizeT index, char* sink, streamsize_t buflen, GuiReadLock& lockHolder, FormattingFlags ff) const override;
	TICTOC_CALL SizeT       AsCharArraySize(SizeT index, streamsize_t maxLen, GuiReadLock& lockHolder, FormattingFlags ff)     const override;
	TICTOC_CALL SharedStr   AsString (SizeT index, GuiReadLock& lockHolder)                                const override;

	TICTOC_CALL std::size_t GetNrTileBytesNow(tile_id t, bool calcStreamSize = false) const override;
	TICTOC_CALL bool        IsSmallerThan(SizeT sz) const override;

//	override AbstrDataObject
	TICTOC_CALL void DoReadData (BinaryInpStream&, tile_id t) override;
	TICTOC_CALL void DoWriteData(BinaryOutStream&, tile_id t) const override;

	TICTOC_CALL virtual auto GetTile(tile_id t) const ->locked_cseq_t = 0;
	TICTOC_CALL virtual auto GetWritableTile(tile_id t, dms_rw_mode rwMode = dms_rw_mode::write_only_all) ->locked_seq_t;

	struct future_tile : abstr_future_tile
	{
		virtual auto GetTile() -> locked_cseq_t = 0;
		auto GetTileCRef() -> TileCRef override {
			auto tile = GetTile();
			return tile.m_TileHolder;
		}
	};

	TICTOC_CALL virtual auto GetFutureTile(tile_id t) const -> SharedPtr<future_tile> = 0;
	auto GetFutureAbstrTile(tile_id t) const -> SharedPtr<abstr_future_tile> override
	{
		return GetFutureTile(t);
	}

	// support for arrays with RangedUnits (now NYI for FPoint, DPoint, polygons and overridden for Bool )
	TICTOC_CALL DataCheckMode DoGetCheckMode() const override;
	TICTOC_CALL DataCheckMode DoDetermineCheckMode() const override;
	TICTOC_CALL void DoSimplifyCheckMode(DataCheckMode& dcm) const override;

//	override Object
	TICTOC_CALL void XML_DumpObjData(OutStreamBase* xmlOutStr, const AbstrDataItem* owner) const override;
	void InitValueRangeData(value_range_ptr_t vrp) { m_ValueRangeDataPtr = std::move(vrp); }

	[[no_unique_address]] value_range_ptr_t m_ValueRangeDataPtr;
	auto GetValueRangeData() const -> const range_or_void_data<field_of_t<V>>*
	{
		if constexpr (has_var_range_v < field_of_t<V>>)
			return m_ValueRangeDataPtr.get_ptr();
		else
		{
			static LifetimeProtector< range_or_void_data<field_of_t<V>> > s_SingletonRangeData;
			return &*s_SingletonRangeData;
		}
	}

	TICTOC_CALL LispRef GetValuesAsKeyArgs(LispPtr valuesUnitKeyExpr) const override;
};

template <> class DataArrayBase<bool> {}; // bool shoudn't be used

//----------------------------------------------------------------------
// class  : NumericArray
//----------------------------------------------------------------------

template <class V> 
struct NumericArray : DataArrayBase<V>
{
	// Support for numerics
	TIC_CALL Float64 GetValueAsFloat64(SizeT index)                       const override;
	TIC_CALL void    SetValueAsFloat64(SizeT index, Float64 val)                override;
	TIC_CALL SizeT   FindPosOfFloat64 (Float64 val, SizeT startPos)       const override;
	TIC_CALL Int32   GetValueAsInt32  (SizeT index)                       const override;
	TIC_CALL void    SetValueAsInt32  (SizeT index, Int32 val)                  override;
	TIC_CALL UInt32  GetValueAsUInt32 (SizeT index)                       const override;
	TIC_CALL void    SetValueAsUInt32 (SizeT index, UInt32 val)                 override;
	TIC_CALL SizeT   GetValueAsSizeT  (SizeT index)                       const override;
	TIC_CALL void    SetValueAsSizeT  (SizeT index, SizeT val)                  override;
	TIC_CALL void    SetValueAsSizeT(SizeT index, SizeT val, tile_id t)         override;
	TIC_CALL UInt8   GetValueAsUInt8  (SizeT index)                       const override;
	TIC_CALL SizeT   FindPosOfSizeT   (SizeT val, SizeT startPos)         const override;

	// Support for numeric arrays
	TIC_CALL SizeT   GetValuesAsFloat64Array(tile_loc tl, SizeT len, Float64* data) const override;
	TIC_CALL SizeT   GetValuesAsUInt32Array (tile_loc tl, SizeT len, UInt32*  data) const override;
	TIC_CALL SizeT   GetValuesAsInt32Array  (tile_loc tl, SizeT len, Int32*   data) const override;
	TIC_CALL SizeT   GetValuesAsUInt8Array  (tile_loc tl, SizeT len, UInt8*   data) const override;

	TIC_CALL void    SetValuesAsFloat64Array(tile_loc tl, SizeT len, const Float64* data) override;
	TIC_CALL void    SetValuesAsInt32Array  (tile_loc tl, SizeT len, const Int32*   data) override;
	TIC_CALL void    SetValuesAsUInt8Array  (tile_loc tl, SizeT len, const UInt8*   data) override;

	TIC_CALL void    FillWithFloat64Values  (tile_loc tl, SizeT len, Float64 fillValue)   override;
	TIC_CALL void    FillWithUInt32Values   (tile_loc tl, SizeT len, UInt32  fillValue)   override;
	TIC_CALL void    FillWithInt32Values    (tile_loc tl, SizeT len, Int32   fillValue)   override;
	TIC_CALL void    FillWithUInt8Values    (tile_loc tl, SizeT len, UInt8   fillValue)   override;

	// Support for value range info
	TIC_CALL row_id GetValuesRangeCount() const override { if constexpr (has_var_range_field_v<V>) return this->m_ValueRangeDataPtr->GetRangeSize();     else return nrbits_of_v<V>; }
	TIC_CALL bool   IsFirstValueZero   () const override { if constexpr (has_var_range_field_v<V>) return this->m_ValueRangeDataPtr->IsFirstValueZero(); else return true; }


	// Helper func
	SizeT FindPos(V val, SizeT startPos = 0) const;
};

//----------------------------------------------------------------------
// class  : AdditiveArray
//----------------------------------------------------------------------

template <class V> 
struct AdditiveArray : NumericArray<V>
{
	// Support for numerics
	TIC_CALL Float64 GetSumAsFloat64() const override;
};

//----------------------------------------------------------------------
// class  : GeoArrayAdapter
//----------------------------------------------------------------------

template <typename Base>
struct GeoArrayAdapter : Base
{
	// Support for Geometrics
	TIC_CALL DRect GetActualRangeAsDRect(bool checkForNulls) const override;
};

//----------------------------------------------------------------------
// class  : PointArrayAdapter
//----------------------------------------------------------------------

template <typename Base>
struct PointArrayAdapter : GeoArrayAdapter<Base>
{
	// Support for GeometricPoints
	TIC_CALL DPoint  GetValueAsDPoint(SizeT index) const override;
	TIC_CALL void    SetValueAsDPoint(SizeT index, const DPoint& val) override;
};

//----------------------------------------------------------------------
// class  : SequenceArrayAdapter
//----------------------------------------------------------------------

template <typename Base>
struct SeqArrayAdapter : GeoArrayAdapter<Base>
{
	// Support for GeometricSequences
	TIC_CALL void GetValueAsDPoints(SizeT index, std::vector<DPoint>& dpoints) const override;
};

//----------------------------------------------------------------------
// class  : data_array_traits
//----------------------------------------------------------------------
template <typename V> struct data_array_traits { typedef DataArrayBase<V>      type; };

template<> struct data_array_traits<UInt64>    { typedef AdditiveArray<UInt64>  type; };
template<> struct data_array_traits<UInt32>    { typedef AdditiveArray<UInt32>  type; };
template<> struct data_array_traits<UInt16>    { typedef AdditiveArray<UInt16>  type; };
template<> struct data_array_traits<UInt8 >    { typedef AdditiveArray<UInt8 >  type; };
template<> struct data_array_traits<Int64>     { typedef AdditiveArray<Int64>   type; };
template<> struct data_array_traits<Int32>     { typedef AdditiveArray<Int32>   type; };
template<> struct data_array_traits<Int16>     { typedef AdditiveArray<Int16>   type; };
template<> struct data_array_traits<Int8 >     { typedef AdditiveArray<Int8 >   type; };
template<> struct data_array_traits<Float32>   { typedef AdditiveArray<Float32> type; };
template<> struct data_array_traits<Float64>   { typedef AdditiveArray<Float64> type; };
template<> struct data_array_traits<Bool>      { typedef AdditiveArray<Bool>    type; };
template<> struct data_array_traits<UInt2 >    { typedef AdditiveArray<UInt2 >  type; };
template<> struct data_array_traits<UInt4 >    { typedef AdditiveArray<UInt4 >  type; };
template<> struct data_array_traits<Void>      {  }; // should not be instantiated

template<> struct data_array_traits<SPoint>    { typedef PointArrayAdapter<NumericArray<SPoint> > type; };
template<> struct data_array_traits<WPoint>    { typedef PointArrayAdapter<NumericArray<WPoint> > type; };
template<> struct data_array_traits<IPoint>    { typedef PointArrayAdapter<NumericArray<IPoint> > type; };
template<> struct data_array_traits<UPoint>    { typedef PointArrayAdapter<NumericArray<UPoint> > type; };
template<> struct data_array_traits<FPoint>    { typedef PointArrayAdapter<DataArrayBase<FPoint> > type; };
template<> struct data_array_traits<DPoint>    { typedef PointArrayAdapter<DataArrayBase<DPoint> > type; };

template<> struct data_array_traits<SPolygon>  { typedef SeqArrayAdapter<DataArrayBase<SPolygon> > type; };
template<> struct data_array_traits<WPolygon>  { typedef SeqArrayAdapter<DataArrayBase<WPolygon> > type; };
template<> struct data_array_traits<IPolygon>  { typedef SeqArrayAdapter<DataArrayBase<IPolygon> > type; };
template<> struct data_array_traits<UPolygon>  { typedef SeqArrayAdapter<DataArrayBase<UPolygon> > type; };
template<> struct data_array_traits<FPolygon>  { typedef SeqArrayAdapter<DataArrayBase<FPolygon> > type; };
template<> struct data_array_traits<DPolygon>  { typedef SeqArrayAdapter<DataArrayBase<DPolygon> > type; };

template <typename V>
struct TileFunctor : data_array_traits<V>::type
{
	TileFunctor() {}

	TileFunctor(const AbstrTileRangeData* tiledDomainRangeData, range_data_ptr_or_void<field_of_t<V>> valueRangePtr MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
#if defined(MG_DEBUG_ALLOCATOR)
		this->md_SrcStr = srcStr;
#endif
		this->m_TileRangeData = tiledDomainRangeData;
		this->InitValueRangeData(valueRangePtr);
	}

	virtual void Commit() { throwIllegalAbstract(MG_POS, "TileFunctor::Commit"); }
#if defined(MG_DEBUG_ALLOCATOR)
	SharedStr md_SrcStr;
#endif
	DECL_RTTI(TIC_CALL, DataItemClass);

};

template <typename V>
struct GeneratedTileFunctor : TileFunctor<V>
{
	using TileFunctor<V>::TileFunctor;
	using future_tile = typename TileFunctor<V>::future_tile;

	struct future_caller : future_tile
	{
		future_caller(tile_id t_, const TileFunctor<V>* self_) : t(t_), self(self_) {}

		auto GetTile() -> TileFunctor<V>::locked_cseq_t override
		{
			return self->GetTile(t);
		}
		tile_id t;
		SharedPtr<const TileFunctor<V>> self;
	};

	auto GetFutureTile(tile_id t) const -> SharedPtr<future_tile> override
	{
		return new future_caller{t, this};
	}
};


//----------------------------------------------------------------------
// GetValue, TODO G8 MOVE to separate header file
//----------------------------------------------------------------------

#include "DataLocks.h"

template <typename V>
auto GetValue(const AbstrDataItem* adi, SizeT idx) -> DataArrayBase<V>::value_type
{
	dms_assert(adi);
	dms_assert(adi->GetInterestCount());
	adi->PrepareDataUsage(DrlType::CertainOrThrow);
	DataReadLock lck(adi);
	return const_array_cast<V>(adi)->GetIndexedValue(idx);
}

template <typename V>
auto GetReference(const AbstrDataItem* adi, SizeT idx) -> DataArrayBase<V>::const_reference
{
	PreparedDataReadLock lck(adi);
	return const_array_cast<V>(adi)->GetIndexedValue(idx);
}

template <typename V>
typename DataArrayBase<V>::value_type GetCurrValue(const AbstrDataItem* adi, SizeT idx)
{
	DataReadLock lck(adi);
	return const_array_cast<V>(adi)->GetIndexedValue(idx);
}

template <typename V>
typename DataArrayBase<V>::value_type GetValue(const TreeItem* ti, SizeT idx)
{
	return GetValue<V>(checked_valcast<const AbstrDataItem*>(ti), idx);
}

template <typename V>
typename DataArrayBase<V>::value_type GetCurrValue(const TreeItem* ti, SizeT idx)
{
	return GetCurrValue<V>(checked_valcast<const AbstrDataItem*>(ti), idx);
}

template <typename V, typename TI>
typename DataArrayBase<V>::value_type GetTheValue(const TI* item)
{
	return GetValue<V>(item, 0);
}

template <typename V, typename TI>
typename DataArrayBase<V>::value_type GetTheCurrValue(const TI* item)
{
	return GetCurrValue<V>(item, 0);
}

//----------------------------------------------------------------------
// GetValue, SetValue, TODO G8 MOVE to separate header file
//----------------------------------------------------------------------


template <typename V>
void SetValue(AbstrDataItem* adi, row_id index, typename DataArrayBase<V>::param_t value)
{
	DataWriteLock dataHolder(adi, dms_rw_mode::read_write);
	dms_assert(adi->m_DataLockCount < 0);

	dataHolder->SetValue<V>(index, value);
	adi->m_DataObject.assign(dataHolder);
}

template <typename V>
void SetTheValue(AbstrDataItem* adi, typename DataArrayBase<V>::param_t value)
{
	DataWriteLock dataHolder(adi);
	MG_CHECK(dataHolder->GetTiledRangeData()->GetRangeSize() == 1);
	dms_assert(adi->m_DataLockCount < 0);

	dataHolder->SetValue<V>(0, value);
	dataHolder.Commit();
}

inline void CheckIndexToTileDataSize(SizeT index, SizeT size)
{
	if (index >= size)
		throwErrorF("DataArrayBase", "Index %d out of array-range (array.size = %d )", index, size);
}


#endif // __TIC_DATAARRAY_H
