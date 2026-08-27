// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  DataArrayBase<V> / DataArray<V>: the typed tile-array interface over
 *  AbstrDataObject -- locked tile access (seq/cseq), value-range data, and the
 *  numeric/geometric value accessors. TileFunctor<V> derives directly from it.
 *  Template member bodies live in DataArray.ipp.
 *
 *  Which accessors apply depends on V: the numeric, point and sequence groups
 *  below are each guarded by a predicate (see "element-group predicates"). A
 *  virtual function cannot carry a requires-clause, so the guard sits in the
 *  body as an `if constexpr` whose else-branch calls the AbstrDataObject
 *  default (which throws or returns the documented neutral value) -- exactly
 *  what a V outside the group inherited when these members still lived in the
 *  NumericArray / AdditiveArray / GeoArrayAdapter / PointArrayAdapter /
 *  SeqArrayAdapter layers that data_array_traits<V> used to splice in.
 *  Non-virtual members (FindPos) do carry a requires-clause.
 */

#if !defined(__TIC_DATAARRAY_H)
#define __TIC_DATAARRAY_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "vt/GeoSequence.h"
#include "vt/SequenceArray.h"
#include "mem/LockedSequenceObj.h"
#include "ptr/LifetimeProtector.h"

#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "TiledRangeData.h"
#include "TileLock.h"

template <typename V>
using value_range_data = std::conditional_t<
	has_range_v<field_of_t<V>>
	, SharedPtr<const range_or_void_data<field_of_t<V>>>
	, Void>;

//----------------------------------------------------------------------
// element-group predicates
//
// These reproduce, per value type, the member sets that the removed
// data_array_traits<V> table used to select by splicing adapter classes:
//   numeric_elem       : the 13 scalar value types (AdditiveArray)
//   countable_point_elem: S/W/I/UPoint       -- index<->value conversion is
//                        meaningful only for integral points, which is why
//                        F/DPoint were never NumericArray
//   numeric_array_api  : NumericArray        = numeric_elem + countable_point_elem
//   geo_elem           : GeoArrayAdapter     = the 6 points + the 6 point polygons
//   point_elem         : PointArrayAdapter   = the 6 point types
//   polygon_elem       : SeqArrayAdapter     = the 6 point polygons
// Note dimension_of_v<SPolygon> == 2 as well (it is a vector of points), so the
// has_fixed_elem_size_v term is what separates points from polygons.
//----------------------------------------------------------------------

template <typename V> constexpr bool numeric_elem_v = (dimension_of_v<V> == 1) && (is_numeric_v<V> || is_bitvalue_v<V>);
template <typename V> constexpr bool geo_elem_v     = (dimension_of_v<V> == 2);
template <typename V> constexpr bool point_elem_v   = geo_elem_v<V> &&  has_fixed_elem_size_v<V>;
template <typename V> constexpr bool polygon_elem_v = geo_elem_v<V> && !has_fixed_elem_size_v<V>;

template <typename V> constexpr bool countable_point_elem_v = point_elem_v<V> && is_integral_v<scalar_of_t<V>>;
template <typename V> constexpr bool numeric_array_api_v    = numeric_elem_v<V> || countable_point_elem_v<V>;

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
	typedef locked_seq< seq_t_HIDE, TileRef>            locked_seq_t;
	typedef locked_seq<cseq_t_HIDE, TileCRef>           locked_cseq_t;

	typedef typename sequence_traits<value_type>::container_type container_t;

	typedef typename container_t::reference                      reference;
	typedef typename container_t::const_reference                const_reference;

	typedef typename seq_t_HIDE::iterator                        iterator;
	typedef typename cseq_t_HIDE::const_iterator                 const_iterator;

	typedef typename field_of<V>::type                           field_t;
	typedef Unit<field_t>                                        unit_t;

	using value_range_ptr_t = range_data_ptr_or_void<field_of_t<V>>;

	TICTOC_CALL AbstrReadableTileData* CreateReadableTileData(tile_id t) const override;

	TIC_CALL auto GetDataRead(tile_id t = no_tile) const -> locked_cseq_t;
	TIC_CALL auto GetDataWrite(tile_id t, dms_rw_mode rwMode) -> locked_seq_t;

	auto GetReadableTileLock(tile_id t) const->TileCRef override { return this->GetTile(t).m_TileHolder; } // TODO G8: REMOVE
	auto GetWritableTileLock(tile_id t, dms_rw_mode rwMode) ->TileRef override { return this->GetWritableTile(t, rwMode).m_TileHolder; } // TODO G8: REMOVE

	auto GetLockedDataRead(tile_id t = no_tile) const { return GetDataRead(t); } // TODO G8: SUBSTITUTE AWAY
	auto GetLockedDataWrite(tile_id t, dms_rw_mode rwMode) { return GetDataWrite(t, rwMode); } // TODO G8: SUBSTITUTE AWAY

	SizeT CountValues(param_t v) const;

//	set data functions
	TIC_CALL void SetIndexedValue(SizeT index, param_t value);
	void SetTileIndexedValue(tile_id t, tile_offset index, param_t value);

//	get data functions
	TIC_CALL value_type     GetIndexedValue   (SizeT index)    const;
	const_iterator GetIndexedIterator(SizeT index, GuiReadLock& lockHolder)    const;

//	override AbstrDataObject
	TICTOC_CALL bool CheckValuesUnit(const AbstrUnit* valuesUnit) override;

	//	Data Access
	TICTOC_CALL auto GetDataReadBegin(tile_id = no_tile) const->data_read_begin_handle override;
	TICTOC_CALL auto GetDataWriteBegin(tile_id, dms_rw_mode rwMode)->data_write_begin_handle override;

	TICTOC_CALL const ValueClass* GetValueClass() const override;
	TICTOC_CALL auto CreateAbstrValue() const ->std::unique_ptr<AbstrValue> override;
	TICTOC_CALL void GetAbstrValue(row_id index, AbstrValue& valueHolder)    const override;
	TICTOC_CALL void SetAbstrValue(row_id index, const AbstrValue& valueHolder)   override;
	TICTOC_CALL void        SetNull      (row_id index)                                 override;
	TICTOC_CALL bool        IsNull       (row_id index) const                           override;
	TICTOC_CALL bool        IsDataRowNull(datarow_id index) const                       override;
	TICTOC_CALL SizeT       GetNrNulls   () const                                       override;
	TICTOC_CALL SizeT       GetNrDataRowNulls() const                                   override;

	TICTOC_CALL bool        AsCharArray(SizeT index, char* sink, streamsize_t buflen, GuiReadLock& lockHolder, FormattingFlags ff) const override;
	TICTOC_CALL SizeT       AsCharArraySize(SizeT index, streamsize_t maxLen, GuiReadLock& lockHolder, FormattingFlags ff)     const override;
	TICTOC_CALL SharedStr   AsString (SizeT index, GuiReadLock& lockHolder, FormattingFlags ff)                                const override;

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

	TICTOC_CALL virtual auto GetFutureTile(tile_id t) const -> std::shared_ptr<future_tile> = 0;
	auto GetFutureAbstrTile(tile_id t) const -> std::shared_ptr<abstr_future_tile> override
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
	auto GetValueRangeData() const -> value_range_data<V>
	{
		if constexpr (has_var_range_v < field_of_t<V>>)
			return m_ValueRangeDataPtr.get_ptr();
		else if constexpr (is_bitvalue_v<field_of_t<V>> || is_void_v<V>)
		{
			static LifetimeProtector< range_or_void_data<field_of_t<V>> > s_SingletonRangeData;
			return &*s_SingletonRangeData;
		}
		else
			return {};
	}
	TICTOC_CALL SharedPtr<const SharedObj> GetAbstrValuesRangeData() const override 
	{ 
		if constexpr (has_var_range_v < field_of_t<V>>)
			return GetValueRangeData();
		return {};
	}

	TICTOC_CALL LispRef GetValuesAsKeyArgs(LispPtr valuesUnitKeyExpr) const override;

//	Support for numerics; applies to numeric_array_api_v<V>, other V delegate to AbstrDataObject
	TIC_CALL Float64 GetValueAsFloat64(SizeT index)                       const override;
	TIC_CALL void    SetValueAsFloat64(SizeT index, Float64 val)                override;
	TIC_CALL SizeT   FindPosOfFloat64 (Float64 val, SizeT startPos)       const override;
	TIC_CALL Int32   GetValueAsInt32  (SizeT index)                       const override;
	TIC_CALL void    SetValueAsInt32  (SizeT index, Int32 val)                  override;
	TIC_CALL UInt32  GetValueAsUInt32 (SizeT index)                       const override;
	TIC_CALL void    SetValueAsUInt32 (SizeT index, UInt32 val)                 override;
	TIC_CALL SizeT   GetValueAsSizeT  (SizeT index)                       const override;
	TIC_CALL void    SetValueAsSizeT  (SizeT index, SizeT val)                  override;
	TIC_CALL void    SetValueAsDiffT  (SizeT index, DiffT val)                  override;
	TIC_CALL void    SetValueAsSizeT  (SizeT index, SizeT val, tile_id t)       override;
	TIC_CALL UInt8   GetValueAsUInt8  (SizeT index)                       const override;
	TIC_CALL SizeT   FindPosOfSizeT   (SizeT val, SizeT startPos)         const override;

//	Support for numeric arrays; applies to numeric_array_api_v<V>
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

//	Support for value range info; applies to numeric_array_api_v<V>
	TIC_CALL row_id GetValuesRangeCount() const override
	{
		if constexpr (numeric_array_api_v<V>)
		{
			if constexpr (has_var_range_field_v<V>) return this->m_ValueRangeDataPtr->GetRangeSize();
			else                                    return nrbits_of_v<V>;
		}
		else
			return AbstrDataObject::GetValuesRangeCount();
	}
	TIC_CALL bool   IsFirstValueZero   () const override
	{
		if constexpr (numeric_array_api_v<V>)
		{
			if constexpr (has_var_range_field_v<V>) return this->m_ValueRangeDataPtr->IsFirstValueZero();
			else                                    return true;
		}
		else
			return AbstrDataObject::IsFirstValueZero();
	}

//	Support for additive numerics: the scalar value types only (not points)
	TIC_CALL Float64 GetSumAsFloat64() const override;

//	Support for Geometrics: points and point polygons
	TIC_CALL DRect GetActualRangeAsDRect(bool checkForNulls) const override;

//	Support for GeometricPoints: the point types only
	TIC_CALL DPoint  GetValueAsDPoint(SizeT index) const override;
	TIC_CALL void    SetValueAsDPoint(SizeT index, const DPoint& val) override;

//	Support for GeometricSequences: the point polygons only
	TIC_CALL void GetValueAsDPoints(SizeT index, std::vector<DPoint>& dpoints) const override;

//	Helper func; non-virtual, so this one does carry its guard as a requires-clause
	SizeT FindPos(V val, SizeT startPos = 0) const requires numeric_array_api_v<V>;
};

template <> class DataArrayBase<bool> {}; // bool shoudn't be used

//----------------------------------------------------------------------
// class  : TileFunctor
//----------------------------------------------------------------------

template <typename V>
struct TileFunctor : DataArrayBase<V>
{
	TileFunctor() {}

	TileFunctor(const AbstrTileRangeData* tiledDomainRangeData, range_data_ptr_or_void<field_of_t<V>> valueRangePtr MG_DEBUG_ALLOCATOR_SRC(SharedStr srcStr))
#if defined(MG_DEBUG_ALLOCATOR)
		: md_SrcStr( std::move(srcStr) )
#endif
	{
		if constexpr (has_var_range_field_v<V>)
		{
			MG_CHECK(valueRangePtr);
		}

		this->m_TileRangeData = tiledDomainRangeData;
		this->InitValueRangeData(valueRangePtr);
	}

#if defined(MG_DEBUG_ALLOCATOR)
	SharedStr md_SrcStr;
#endif
	DECL_RTTI(TIC_CALL, DataItemClass)

};

// Void specialization: should never be instantiated in practice
template <>
struct TileFunctor<Void> : AbstrDataObject
{
	TileFunctor() {}
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

	auto GetFutureTile(tile_id t) const -> std::shared_ptr<future_tile> override
	{
		return std::make_shared<future_caller>(t, this);
	}
};

//----------------------------------------------------------------------
// DataLocks (DataReadLock / DataWriteLock / DrlType): the value-accessor free
// functions that used these moved to DataArrayValue.h, but many tic TUs still
// obtain DataLocks transitively via DataArray.h, so the include is kept here.
//----------------------------------------------------------------------

#include "DataLocks.h"

//----------------------------------------------------------------------
// low-level helper (used by DataArray.ipp)
//----------------------------------------------------------------------

inline void CheckIndexToTileDataSize(SizeT index, SizeT size)
{
	if (index >= size)
		throwErrorF("DataArrayBase", "Index {} out of array-range (array.size = {} )", index, size);
}

#endif // __TIC_DATAARRAY_H
