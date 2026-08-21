// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  Unit<V>: the typed unit template -- holds the value range and tiling
 *  (TiledRangeData) of a domain or values unit, with checked cardinality
 *  calculation and range formatting/streaming support.
 *
 *  Unit<V> derives directly from AbstrUnit. Which part of its interface applies
 *  depends on V, expressed by the element-group predicates below rather than by
 *  the chain of intermediate classes (UnitBase, RangedUnit, FloatUnit,
 *  CountableUnitBase, BitUnitBase, VoidUnitBase and the Num/Geo/Tile/Indexable
 *  adapters) that unit_traits<V> used to splice together -- those existed only to
 *  express conditional members before constexpr and requires were available.
 *
 *  The non-virtual typed API carries requires-clauses. Virtual overrides cannot
 *  ([class.virtual]/6), so they are declared unconditionally and guard their body
 *  with `if constexpr`, delegating to the AbstrUnit default (which throws, or
 *  returns the documented neutral value) for a V outside the group -- exactly what
 *  such a V got before by not having the member spliced in at all.
 */

#if !defined(__TIC_UNIT_H)
#define __TIC_UNIT_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "vt/CheckedCalc.h"
#include "ser/RangeStream.h"

#include "AbstrUnit.h"
#include "TiledRangeData.h"

//----------------------------------------------------------------------
// element-group predicates
//
// These reproduce, per value type, the class chains that unit_traits<V> selected:
//   ranged_unit_v      : RangedUnit        = ints, floats, all 6 point types
//   simple_range_unit_v: FloatUnit         = floats and float points (no tiling)
//   countable_unit_v   : CountableUnitBase = ints and integral point types
//   tileable_unit_v    : TileAdapter       = countable with sizeof > 2
//                        (== typelists::tiled_domain_elements)
//   fixed_range_unit_v : BitUnitBase / VoidUnitBase = Bool, UInt2, UInt4, Void
//   indexable_unit_v   : IndexableUnitAdapter = countable + fixed-range
//   num_range_unit_v   : NumRangeUnitAdapterBase = the 1-dimensional ranges
//   ordinal_unit_v     : OrderedUnit      = 1-dimensional countable
//   geo_unit_v         : GeoUnitAdapter   = the 6 point types
// SharedStr satisfies none of them and keeps the bare AbstrUnit behaviour, which
// is what unit_traits<SharedStr> == UnitBase<SharedStr> amounted to.
//----------------------------------------------------------------------

template <typename V> constexpr bool ranged_unit_v       = has_var_range_v<V>;
template <typename V> constexpr bool simple_range_unit_v = ranged_unit_v<V> &&  has_simple_range_v<V>;
template <typename V> constexpr bool countable_unit_v    = ranged_unit_v<V> && !has_simple_range_v<V>;
template <typename V> constexpr bool tileable_unit_v     = countable_unit_v<V> && !has_small_range_v<V>;
template <typename V> constexpr bool fixed_range_unit_v  = is_bitvalue_v<V> || is_void_v<V>;
template <typename V> constexpr bool indexable_unit_v    = countable_unit_v<V> || fixed_range_unit_v<V>;
template <typename V> constexpr bool num_range_unit_v    = (dimension_of_v<V> == 1) && (ranged_unit_v<V> || fixed_range_unit_v<V>);
template <typename V> constexpr bool ordinal_unit_v      = countable_unit_v<V> && (dimension_of_v<V> == 1);
template <typename V> constexpr bool geo_unit_v          = (dimension_of_v<V> == 2);

//----------------------------------------------------------------------
// class  : Unit<V>
//----------------------------------------------------------------------

template <class V>
class Unit : public AbstrUnit
{
	using base_type = AbstrUnit;
public:
	using value_t      = V;
	using range_data_t = range_or_void_data<V>;                                              // FixedRange<N> for bits, FixedRange<0> for Void, Void for SharedStr
	using range_t      = Range<std::conditional_t<fixed_range_unit_v<V>, UInt32, V>>;        // only named, never completed, for V without a range
	using extent_t     = tile_extent_t<V>;

//	Range data; public because get_range_ptr_of_valuesunit reads it through Unit<E>*
	[[no_unique_address]] range_data_ptr_or_void<V> m_RangeDataPtr;

//	Typed range API; non-virtual, so these carry their guard as a requires-clause
	TIC_CALL auto GetSegmInfo()     const -> const range_data_t* requires (ranged_unit_v<V> || fixed_range_unit_v<V>);
	TIC_CALL auto GetCurrSegmInfo() const -> const range_data_t* requires (ranged_unit_v<V> || fixed_range_unit_v<V>);

	TIC_CALL range_t GetRange()         const requires (ranged_unit_v<V> || fixed_range_unit_v<V>);
	TIC_CALL range_t GetPreparedRange() const requires ranged_unit_v<V>;
	TIC_CALL void SetRange(const range_t& range)                       requires ranged_unit_v<V>;
	TIC_CALL void SetRange(const range_t& range, extent_t blockSize)   requires ranged_unit_v<V>;
	TIC_CALL void ValidateRange(const range_t& range) const            requires ranged_unit_v<V>;

	TIC_CALL void SetIrregularTileRange(std::vector<range_t> optionalTileRanges) requires tileable_unit_v<V>;
	TIC_CALL void SetRegularTileRange(const range_t& range, extent_t tileExtent) requires tileable_unit_v<V>;

	TIC_CALL range_t GetTileRange(tile_id t)          const requires indexable_unit_v<V>;
	TIC_CALL value_t GetValueAtIndex(row_id i)        const requires indexable_unit_v<V>;
	TIC_CALL row_id  GetIndexForValue(const value_t&) const requires indexable_unit_v<V>;

	V GetTileFirstValue(tile_id t)                    const requires ordinal_unit_v<V>;
	V GetTileValue(tile_id t, tile_offset localIndex) const requires ordinal_unit_v<V>;

//	override AbstrUnit / TreeItem: declared for every V, guarded in the body
	LispRef GetKeyExprImpl() const override;
	bool HasTiledRangeData() const override { if constexpr (has_var_range_field_v<V>) return m_RangeDataPtr; return true; }
	bool HasVarRange() const override { return ranged_unit_v<V>; }
	bool CanBeDomain() const override { return indexable_unit_v<V>; }

	TIC_CALL void SetMaxRange() override;
	TIC_CALL SharedStr GetRangeAsStr(FormattingFlags ff) const override;
	void ClearDataObject(garbage_can& g) const override;
	void CopyProps(TreeItem* result, const CopyTreeContext& copyContext) const override;
	void LoadBlobStream (const InpStreamBuff* is) override;
	void StoreBlobStream(      OutStreamBuff* os) const override;

	auto GetTiledRangeData() const -> SharedPtr<const AbstrTileRangeData> override;
	bool IsTiled() const override;
	bool IsCurrTiled() const override;
	bool ContainsUndefined(tile_id t) const override;

	row_id GetBase() const override;
	row_id GetCount() const override;
	row_id GetDataCount() const override;
	row_id GetPreparedCount(bool throwOnUndefined = true) const override;
	tile_offset GetPreparedTileCount(tile_id t) const override;
	tile_offset GetTileCount(tile_id t) const override;
	void SetCount(SizeT) override;

	row_id GetDimSize(DimType dimNr) const override;
	auto CreateAbstrValueAtIndex(SizeT i) const -> std::unique_ptr<AbstrValue> override;
	SizeT GetIndexForAbstrValue(const AbstrValue&) const override;

//	Support for Numerics: the 1-dimensional ranges
	Range<Float64> GetRangeAsFloat64() const override;
	TIC_CALL void SetRangeAsFloat64(Float64 begin, Float64 end) override;
	TIC_CALL void SetRangeAsUInt64 (UInt64  begin, UInt64  end) override;

//	Support for Geometrics: the point types
	IRect   GetRangeAsIRect() const override;
	DRect   GetRangeAsDRect() const override;
	IRect   GetTileRangeAsIRect(tile_id t) const override;
	I64Rect GetTileSizeAsI64Rect(tile_id t) const override;
	void SetRangeAsIPoint(Int32 rowBegin, Int32 colBegin, Int32 rowEnd, Int32 colEnd, UInt16 blockSizeY, UInt16 blockSizeX) override;
	void SetRangeAsDPoint(Float64 rowBegin, Float64 colBegin, Float64 rowEnd, Float64 colEnd) override;

//	Metric and projection; both are answered for the referred item when there is one
	const UnitMetric*     GetMetric()         const override;
	const UnitMetric*     GetCurrMetric()     const override;
	void SetMetric(SharedPtr<const UnitMetric> m) override;                 // only from Update or Create
	const UnitProjection* GetProjection()     const override;
	const UnitProjection* GetCurrProjection() const override;
	void SetProjection(SharedPtr<const UnitProjection> p) override;         // only from Update or Create

//	implement AbstrUnit virtuals
	DimType GetNrDimensions() const override;

//	Visitor support
	void InviteUnitProcessor(const UnitProcessor& visitor) const override;

	~Unit(); // out of line: hides the dtors of SharedPtr<const UnitMetric> / <const UnitProjection>

private:
	void LoadRangeImpl (BinaryInpStream&);
	void StoreRangeImpl(BinaryOutStream&) const;

	[[no_unique_address]] mutable std::conditional_t<ranged_unit_v<V>, SharedPtr<const UnitMetric>,     Void> m_Metric;
	[[no_unique_address]] mutable std::conditional_t<geo_unit_v<V>,    SharedPtr<const UnitProjection>, Void> m_Projection;

	friend Object* CreateFunc<Unit<V> >();
	Unit();

	DECL_RTTI(TIC_CALL, UnitClass)
};

template <typename V>
class Unit<const V> {}; // unsupported use

template <typename E>
auto get_range_ptr_of_valuesunit(const Unit<E>* valuesUnitPtr)
{
//	assert(!valuesUnitPtr || valuesUnitPtr->GetCurrRangeItem() == valuesUnitPtr);

	if constexpr (has_var_range_field_v<E>)
	{
		if (valuesUnitPtr)
		{
			dbg_assert(valuesUnitPtr->CheckMetaInfoReadyOrPassor());

			valuesUnitPtr = const_unit_cast<E>(valuesUnitPtr->GetCurrRangeItem()); // Or fix it here
			if (!valuesUnitPtr->m_RangeDataPtr)
				valuesUnitPtr = nullptr;
		}
	}
	if (!valuesUnitPtr)
		valuesUnitPtr = const_unit_cast<E>(Unit<E>::GetStaticClass()->CreateDefault());
	assert(valuesUnitPtr);
	return valuesUnitPtr->m_RangeDataPtr;
}


#endif // __TIC_UNIT_H
