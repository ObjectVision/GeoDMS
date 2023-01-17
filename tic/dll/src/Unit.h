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

#if !defined(__TIC_UNIT_H)
#define __TIC_UNIT_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "geo/RangeIndex.h"
#include "ptr/LifetimeProtector.h"
#include "ser/PointStream.h"
#include "ser/RangeStream.h"
#include "geo/SequenceArray.h"

#include "AbstrUnit.h"
#include "TiledRangeData.h"

//----------------------------------------------------------------------
// class  : Unit<value_t>
//----------------------------------------------------------------------

/*
unit_traits<T>::type tells which base type is used for a Unit<T>

rangedUnit<T> isa abstrUnit with range<T>

boolUnit isa abstrUnit
  with GetCount impl

voidUnit isa abstrUnit
  with GetCount impl

countableUnit isa rangedUnit
  with GetCount impl

ordinalUnit isa countableUnit
  with SetCount impl
  T = unsigned int 32/16/8
*/

//----------------------------------------------------------------------
template <class V>
struct UnitBase : AbstrUnit
{
	using value_t = V;
	using range_data_t = range_or_void_data<value_t>;

	LispRef GetKeyExprImpl() const override;

	bool HasTiledRangeData() const override { if constexpr (has_var_range_field_v<V>) return m_RangeDataPtr; return true; }

	[[no_unique_address]] range_data_ptr_or_void<V> m_RangeDataPtr; // , mc_RangeDataPtr;
};

template <class V> 
struct RangedUnit : UnitBase<V>
{
	using value_t = V;
	using range_t = Range<value_t>;
	using range_data_t = range_or_void_data<V>;

	TIC_CALL auto GetSegmInfo() const -> const range_data_t*;
	TIC_CALL auto GetCurrSegmInfo() const -> const range_data_t*;

	//	data member access
	TIC_CALL range_t GetPreparedRange() const;
	TIC_CALL range_t GetRange() const;
	TIC_CALL virtual void SetRange(const range_t& range) = 0;

	void  ClearData(garbage_t& g) const override;

	TIC_CALL void ValidateRange(const range_t& range) const;

//	Override TreeItem virtuals
	void CopyProps(TreeItem* result, const CopyTreeContext& copyContext) const override;

//	override AbstrUnit
	void LoadBlobStream  (const InpStreamBuff* is) override;
	void StoreBlobStream (      OutStreamBuff* os) const override;

	bool HasVarRange() const override { return true; }

	SharedStr GetRangeAsStr() const override { return AsString(GetRange()); }

	const UnitMetric* GetMetric() const override;
	const UnitMetric* GetCurrMetric() const override;

//	mag alleen vanuit Update of Create worden aangeroepen 
	void SetMetric(SharedPtr < const UnitMetric> m) override;
protected:
	virtual void LoadRangeImpl (BinaryInpStream&);
	virtual void StoreRangeImpl(BinaryOutStream&) const;

private:
	mutable SharedPtr<const UnitMetric> m_Metric;
};

template <class V>
struct FloatUnit : RangedUnit<V>
{
	using range_t = Range<V>;

	TIC_CALL void SetRange(const range_t& range) override;
	TIC_CALL void SetMaxRange() override;
};

//----------------------------------------------------------------------

template <typename U> 
struct NumRangeUnitAdapterBase : U // all numerics elements
{
// Support for Numerics
	Range<Float64> GetRangeAsFloat64() const override;
};

template <typename U> 
struct VarNumRangeUnitAdapter : NumRangeUnitAdapterBase<U> // all numeric objects, but not the subbyte elements
{
	static_assert(has_var_range_field_v<U>);
	// Support for Numerics
	TIC_CALL void SetRangeAsFloat64(Float64 begin, Float64 end) override;
};

template <typename U> 
struct FixedNumRangeUnitAdapter : NumRangeUnitAdapterBase<U> // all subbyte elements
{
	static_assert(has_var_range_field_v<U>);
	FixedNumRangeUnitAdapter() {} // this->SetDataInMem();
	row_id GetBase() const override { return 0; }
	SizeT GetCount() const override { return Cardinality(this->GetRange()); }
};

//----------------------------------------------------------------------
template <typename U>
struct GeoUnitAdapter : U // all integral and float Point types
{
//	static_assert(has_var_range_field_v<U>);

	~GeoUnitAdapter(); // hide dtor of SharedPtr<const UnitProjection>
// Support for Countable Geometrics
	IRect GetRangeAsIRect() const override;
	void SetRangeAsIPoint(Int32  rowBegin, Int32  colBegin, Int32  rowEnd, Int32  colEnd) override;

// Support for Geometrics
	DRect GetRangeAsDRect() const override;
	void SetRangeAsDPoint(Float64  rowBegin, Float64  colBegin, Float64  rowEnd, Float64  colEnd) override;

	I64Rect GetTileSizeAsI64Rect(tile_id t) const override;

	const UnitProjection* GetProjection() const override;
	const UnitProjection* GetCurrProjection() const override;

//	Override TreeItem virtuals
	void CopyProps(TreeItem* result, const CopyTreeContext& copyContext) const override;

// mag alleen vanuit Update of Create worden aangeroepen 
	void SetProjection(SharedPtr < const UnitProjection> p) override;
private:
	mutable SharedPtr<const UnitProjection> m_Projection;
};

template <typename U>
struct GeoUnitAdapterI : GeoUnitAdapter<U> // all integral Point types, but not the float Point types
{
	IRect GetTileRangeAsIRect(tile_id t) const override;
};

//----------------------------------------------------------------------

template <typename V> 
struct CountableUnitBase : RangedUnit<V> // all integral objects and integral point types
{
	using base_type = RangedUnit<V>;
	using typename base_type::value_t;
	using typename base_type::range_t;

	TIC_CALL void SetRange(const range_t& range) override;
	TIC_CALL void SetMaxRange() override;

	auto GetTiledRangeData() const  -> const AbstrTileRangeData* override;

	bool IsTiled() const override;
	bool IsCurrTiled() const override;
//	tile_id GetTileID(SizeT& index) const override;
//	tile_id GetThisCurrTileID(SizeT& index, tile_id prevT) const override;
	TIC_CALL auto GetTileRange(tile_id t) const -> range_t;

	row_id GetPreparedCount(bool throwOnUndefined = true) const override;
	row_id GetCount() const override;
	tile_offset GetPreparedTileCount(tile_id t) const override;
	tile_offset GetTileCount(tile_id t) const override;

	value_t GetValueAtIndex(row_id i) const;
	row_id  GetIndexForValue(const value_t&) const;

protected:
	void LoadRangeImpl (BinaryInpStream& pis) override;
	void StoreRangeImpl(BinaryOutStream& pos) const override;
	bool ContainsUndefined(tile_id t) const override;
};

template <typename Base>
struct TileAdapter : Base
{
	using typename Base::value_t;
	using typename Base::range_t;
	using extent_t = tile_extent_t<value_t>;

	TIC_CALL void SetIrregularTileRange(std::vector<range_t> optionalTileRanges);
	TIC_CALL void SetRegularTileRange(const range_t& range, extent_t tileExtent);
};

//----------------------------------------------------------------------
template <typename U> 
struct IndexableUnitAdapter : U
{
	bool   CanBeDomain() const override { return true; }

	row_id GetDimSize(DimType dimNr) const override;

	// use U::GetValueAtIndex and U:GetIndexForValue
	AbstrValue* CreateAbstrValueAtIndex(SizeT i) const override;
	SizeT GetIndexForAbstrValue(const AbstrValue&) const override;
};


//----------------------------------------------------------------------
template <typename V> 
struct CountableUnit : IndexableUnitAdapter< CountableUnitBase<V> > 
{};

//----------------------------------------------------------------------
template <class V> 
struct OrderedUnit : CountableUnit<V>
{
	static_assert(std::is_integral_v<V>);

	row_id GetBase() const override { return this->GetRange().first; }
	// 
	//	Support for OrderedUnits
	void Split   (row_id pos, row_id len) override;
	void Merge   (row_id pos, row_id len) override;

	V GetTileFirstValue (tile_id t) const;
	V GetTileValue (tile_id t, tile_offset localIndex) const;
};


//----------------------------------------------------------------------
template <class V> 
struct OrdinalUnit : OrderedUnit<V>
{
	static_assert(std::is_integral_v<V>);
	static_assert(std::is_unsigned_v<V>);

	//	Support for Ordinals
	void SetCount(SizeT) override;
//	void MakeTiledUnitFromSize(SizeT count, SizeT maxTileSize = MAX_TILE_SIZE) override;
};

//----------------------------------------------------------------------
template <bit_size_t N>
struct BitUnitBase : UnitBase<bit_value<N>>
{
//	static_assert(has_var_range_field_v<U>);

	typedef bit_value<N>  value_t;
	typedef Range<UInt32> range_t;

	static const UInt32 elem_count = mpf::exp2<N>::value;

	auto GetTiledRangeData() const  -> const AbstrTileRangeData* override { 
		static SharedPtr<FixedRange<N>> s_RangeData = new FixedRange<N>;
		return s_RangeData;
	}

	range_t GetRange() const { return range_t(0, elem_count); }
//	range_t GetPreparedRange() const { return GetRange(); }

	range_t GetTileRange(tile_id t) const { dms_assert(t == 0); return range_t(0, elem_count); }
//	range_t GetPreparedTileRange(tile_id t) const { return GetTileRange(t); }

	SharedStr GetRangeAsStr() const override { return AsString(GetRange()); }

//	Support for Numerics; TODO merge this func with the NumericUnitAdapter version
	value_t GetValueAtIndex (SizeT   i) const { return i; }
	SizeT  GetIndexForValue(value_t v) const { return v; }
};

//----------------------------------------------------------------------
struct VoidUnitBase : UnitBase<Void>
{
	static_assert(!has_var_range_field_v<Void>);

	typedef Void          value_t;
	typedef Range<UInt32> range_t;

	auto GetTiledRangeData() const  -> const AbstrTileRangeData* override {
		static SharedPtr<FixedRange<0>> s_RangeData = new FixedRange<0>;
		return s_RangeData;
	}

	range_t GetRange() const { return range_t(0, 1); }
	range_t GetTileRange(tile_id t) const { dms_assert(t==0); return range_t(0, 1); }

// Support for Numerics
	value_t GetValueAtIndex (row_id   i) const { dms_assert(!i); return Void(); }
	row_id  GetIndexForValue(value_t v) const { return 0; }
};

template <bit_size_t N>
struct BitUnit : IndexableUnitAdapter< FixedNumRangeUnitAdapter<BitUnitBase<N> > >
{};

typedef IndexableUnitAdapter<FixedNumRangeUnitAdapter< VoidUnitBase> > VoidUnit;

//----------------------------------------------------------------------

template <typename V> struct unit_traits;
template<> struct unit_traits<UInt64>    { typedef TileAdapter < VarNumRangeUnitAdapter<OrdinalUnit<UInt64> >> type; };
template<> struct unit_traits<UInt32>    { typedef TileAdapter < VarNumRangeUnitAdapter<OrdinalUnit<UInt32> >> type; };
template<> struct unit_traits<UInt16>    { typedef VarNumRangeUnitAdapter<OrdinalUnit<UInt16> > type; };
template<> struct unit_traits<UInt8 >    { typedef VarNumRangeUnitAdapter<OrdinalUnit< UInt8> > type; };
template<> struct unit_traits<Int64>     { typedef TileAdapter < VarNumRangeUnitAdapter<OrderedUnit< Int64>> > type; };
template<> struct unit_traits<Int32>     { typedef TileAdapter < VarNumRangeUnitAdapter<OrderedUnit< Int32>> > type; };
template<> struct unit_traits<Int16>     { typedef VarNumRangeUnitAdapter<OrderedUnit< Int16> > type; };
template<> struct unit_traits<Int8 >     { typedef VarNumRangeUnitAdapter<OrderedUnit<  Int8> > type; };
template<> struct unit_traits<Float32>   { typedef VarNumRangeUnitAdapter<FloatUnit<Float32> >  type; };
template<> struct unit_traits<Float64>   { typedef VarNumRangeUnitAdapter<FloatUnit<Float64> >  type; };
#if defined(DMS_TM_HAS_FLOAT80)
template<> struct unit_traits<Float80>   { typedef VarNumRangeUnitAdapter<RangedUnit<Float80> > type; };
#endif
template<> struct unit_traits<Bool>      { typedef BitUnit<1>                                type; };
template<> struct unit_traits<UInt2>     { typedef BitUnit<2>                                type; };
template<> struct unit_traits<UInt4>     { typedef BitUnit<4>                                type; };
template<> struct unit_traits<Void>      { typedef VoidUnit                                  type; };
template<> struct unit_traits<SPoint>    { typedef GeoUnitAdapterI< TileAdapter<CountableUnit<SPoint> > > type; };
template<> struct unit_traits<IPoint>    { typedef GeoUnitAdapterI< TileAdapter<CountableUnit<IPoint> > > type; };
template<> struct unit_traits<WPoint>    { typedef GeoUnitAdapterI< TileAdapter<CountableUnit<WPoint> > > type; };
template<> struct unit_traits<UPoint>    { typedef GeoUnitAdapterI< TileAdapter<CountableUnit<UPoint> > > type; };
template<> struct unit_traits<FPoint>    { typedef GeoUnitAdapter<FloatUnit<FPoint>    >     type; };
template<> struct unit_traits<DPoint>    { typedef GeoUnitAdapter<FloatUnit<DPoint>    >     type; };
template<> struct unit_traits<SharedStr> { typedef UnitBase<SharedStr>                       type; };

template <class V>
class Unit : public unit_traits<V>::type
{
	using base_type = AbstrUnit;
public:
	using value_t = V;
//	implement AbstrUnit virtuals
	DimType GetNrDimensions() const override;

//	Visitor support
	void InviteUnitProcessor(const UnitProcessor& visitor) const override;

private: friend Object* CreateFunc<Unit<V> >();
	Unit();

	DECL_RTTI(TIC_CALL, UnitClass);
};

template <typename V>
class Unit<const V> {}; // unsupported use

template <typename E>
auto get_range_ptr_of_valuesunit(const Unit<E>* valuesUnitPtr)
{
	// DEBUG TESTS
//	dms_assert(valuesUnitPtr);
	dms_assert(!valuesUnitPtr || valuesUnitPtr->GetCurrRangeItem() == valuesUnitPtr);

	if constexpr (has_var_range_field_v<E>)
	{
		if (valuesUnitPtr)
		{
			dbg_assert(valuesUnitPtr->CheckMetaInfoReadyOrPassor());
			dms_assert(valuesUnitPtr == valuesUnitPtr->GetCurrRangeItem()); // PRECONDITION ? REMOVE !
			dms_assert(valuesUnitPtr->m_RangeDataPtr); // DEBUG TEST

			valuesUnitPtr = const_unit_cast<E>(valuesUnitPtr->GetCurrRangeItem()); // Or fix it here
			if (!valuesUnitPtr->m_RangeDataPtr)
				valuesUnitPtr = nullptr;
		}
	}
	if (!valuesUnitPtr)
		valuesUnitPtr = const_unit_cast<E>(Unit<E>::GetStaticClass()->CreateDefault());
	dms_assert(valuesUnitPtr);
	return valuesUnitPtr->m_RangeDataPtr;
}


#endif // __TIC_UNIT_H
