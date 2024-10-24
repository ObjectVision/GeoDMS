// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__TIC_TILERANGEDATA_H)
#define __TIC_TILERANGEDATA_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "geo/CheckedCalc.h"
#include "geo/Conversions.h"
#include "geo/Point.h"
#include "geo/Range.h"
#include "geo/RangeIndex.h"
#include "ser/FileMapHandle.h"

//----------------------------------------------------------------------

enum class tile_type_id { none, simple, small_, default_, regular, irregular, max };

using tile_loc = std::pair<tile_id, tile_offset>;
using hash_code = UInt32;

template <typename V>
using tile_extent_t = std::conditional_t< is_numeric_v<V>, UInt32, WPoint>;

template <typename V>
struct SimpleRangeData : SharedObj
{
	static_assert(std::is_floating_point_v<scalar_of_t<V>>);

	SimpleRangeData() = default;
	SimpleRangeData(const Range<V>& range) : m_Range(range) {}

	void Load(BinaryInpStream& pis);
	void Save(BinaryOutStream& pis) const;
	tile_type_id GetTileTypeID() const { return tile_type_id::simple; }

	Range<V> GetRange() const
	{
		return m_Range;
	}
	row_id GetRangeSize() const { return Cardinality(GetRange()); }
	row_id GetDataSize() const { return GetRangeSize(); }

	bool IsFirstValueZero() const
	{
		if constexpr (dimension_of_v<V> == 2)
			return false;
		else
			return m_Range.first == 0;
	}

	auto GetAsLispRef(LispPtr base, bool asCategorical) const->LispRef;

	Range<V> m_Range;
};

struct AbstrTileRangeData : SharedObj
{
	virtual tile_id GetNrTiles() const = 0;
	virtual SizeT GetNrMemPages(UInt8 log2BytesPerElem) const = 0;
	virtual SizeT GetMemPageIndex(UInt8 log2BytesPerElem, tile_id t) const = 0;

	virtual tile_offset GetTileSize(tile_id t) const = 0;
	virtual tile_offset GetMaxTileSize() const = 0;
	virtual bool IsCovered() const { return true; }
	virtual tile_loc GetTiledLocation(row_id index) const = 0;
	virtual tile_loc GetTiledLocation(row_id index, tile_id /*prevT*/) const { return GetTiledLocation(index); }
	virtual row_id GetRangeSize() const = 0;
	virtual row_id GetDataSize() const { return GetRangeSize(); }

	virtual I64Rect GetRangeAsI64Rect() const = 0;
	virtual I64Rect GetTileRangeAsI64Rect(tile_id t) const = 0;
	virtual row_id  GetFirstRowIndex(tile_id t) const = 0;
	virtual row_id  GetRowIndex(tile_id t, tile_offset localIndex) const = 0;

	virtual void Load(BinaryInpStream& /*pis*/) {}
	virtual void Save(BinaryOutStream& /*pos*/) const {}

	virtual LispRef GetAsLispRef(LispPtr base, bool asCategorical) const = 0;

	TIC_CALL hash_code GetHashCode() const;
	TIC_CALL row_id GetElemCount() const;
};

template <typename V>
struct SmallRangeData : AbstrTileRangeData
{
	static_assert(has_small_range_v<V>);
	static_assert(sizeof(V) <= 2);

	SmallRangeData() = default;
	SmallRangeData(const Range<V>& range) : m_Range(range) {}

	void Load(BinaryInpStream& pis);
	void Save(BinaryOutStream& pis) const;
	tile_type_id GetTileTypeID() const { return tile_type_id::small_; }

	Range<V> GetRange() const
	{
		return m_Range;
	}
	row_id GetRangeSize() const override { return Cardinality(GetRange()); }

	SizeT GetNrMemPages(UInt8 log2BytesPerElem) const override
	{
		return NrMemPages(GetElemCount() << log2BytesPerElem);
	}
	SizeT GetMemPageIndex(UInt8 log2BytesPerElem, tile_id t) const override
	{
		return 0;
	}

	tile_id GetNrTiles() const override { return 1; }
	tile_offset GetTileSize(tile_id t) const override { assert(t == 0); return GetRangeSize(); }
	tile_offset GetMaxTileSize() const override { return GetRangeSize(); }
	tile_loc GetTiledLocation(row_id index) const override { return { 0, index }; }

	I64Rect GetRangeAsI64Rect() const override { return { {0, 0}, shp2dms_order(GetRangeSize(), row_id(1))}; }
	I64Rect GetTileRangeAsI64Rect(tile_id t) const { assert(t == 0 || t == no_tile); return GetRangeAsI64Rect(); }
	row_id GetFirstRowIndex(tile_id t) const override { assert(t == 0); return 0; }
	row_id GetRowIndex(tile_id t, tile_offset localIndex) const override { assert(t == 0);  return localIndex; }

	// range_t(dependent on T) specific functions, non virtual
	Range<V> GetTileRange(tile_id t) const { assert(t == 0); return GetRange(); }
	row_id GetElemCount() const { return GetRangeSize(); }
	bool IsFirstValueZero() const { return m_Range.first == 0; }

	auto GetAsLispRef(LispPtr base, bool asCategorical) const -> LispRef override;

	V GetTileValue(tile_id t, tile_offset localIndex) const
	{
		assert(t == 0);
		return Range_GetValue_checked(m_Range, localIndex);
	}

	Range<V> m_Range;
};

template <bit_size_t N>
struct FixedRange : AbstrTileRangeData
{
	auto GetRange() const {
		return Range<UInt32>(0, 1 << N);
	}
	tile_id GetNrTiles() const override { return 1; }
	tile_offset GetTileSize(tile_id t) const override { assert(t == 0); return GetRangeSize(); }
	tile_offset GetMaxTileSize() const override { return GetRangeSize(); }
	tile_loc GetTiledLocation(row_id index) const override { assert(index < (1 << N)); return { 0, index }; }
	row_id GetRangeSize() const override { return 1 << N; }

	SizeT GetNrMemPages(UInt8 log2BytesPerElem) const override
	{
		return NrMemPages(GetElemCount() << log2BytesPerElem);
	}
	SizeT GetMemPageIndex(UInt8 log2BytesPerElem, tile_id t) const override
	{
		return 0;
	}


	I64Rect GetRangeAsI64Rect() const override { return { {0, 0}, shp2dms_order(1 << N, 1) }; }
	I64Rect GetTileRangeAsI64Rect(tile_id t) const { assert(t == 0 || t==no_tile); return GetRangeAsI64Rect(); }
	row_id GetFirstRowIndex(tile_id t) const override { assert(t == 0); return 0; }
	row_id GetRowIndex(tile_id t, tile_offset localIndex) const override { assert(t == 0);  return localIndex; }

	// range_t(dependent on T) specific functions, non virtual
	Range<UInt32> GetTileRange(tile_id t) const { assert(t == 0); return GetRange(); }
	row_id GetElemCount() const { return GetRangeSize(); }
	bool IsFirstValueZero() const { return true; }

	bit_value<N> GetTileValue(tile_id t, tile_offset localIndex) const
	{
		assert(t == 0);
		assert(localIndex < (1 << N));
		return localIndex;
	}

	LispRef GetAsLispRef(LispPtr base, bool /*asCategorical*/) const override { return base; }
};

template <typename V>
struct TiledRangeData : AbstrTileRangeData
{
	using value_type = V;
	using range_type = Range<value_type>;

	TiledRangeData() = default;
	TiledRangeData(const Range<V>& range) : m_Range(range) {}

	void Load(BinaryInpStream& pis) override;
	void Save(BinaryOutStream& pis) const override;
	virtual void CalcTilingExtent() {}

	virtual tile_type_id GetTileTypeID() const { return tile_type_id::simple; }

	range_type GetRange() const { return m_Range;}
	row_id GetRangeSize() const override { return Cardinality(GetRange()); }

	virtual auto GetTileRange   (tile_id t) const -> range_type = 0;
	virtual auto GetTilingExtent() const -> tile_extent_t<value_type> = 0;

	row_id GetIndexForValue(V v) const
	{
		return Range_GetIndex_checked(GetRange(), v);
	}
	V GetValueForIndex(row_id i) const
	{
		return Range_GetValue_checked(GetRange(), i);
	}

	I64Rect GetRangeAsI64Rect() const override
	{
		if constexpr (is_numeric_v<V>)
		{
			auto range = GetRange();
			SizeT nrRows = range.empty() ? 0 : 1;
			return I64Rect(shp2dms_order<Int64>(range.first, 0), shp2dms_order<Int64>(range.second, nrRows));

		}
		else
			return ThrowingConvert<I64Rect>(GetRange());
	}
	I64Rect GetTileRangeAsI64Rect(tile_id t) const override
	{
		if constexpr (is_numeric_v<V>)
		{
			auto range = (t==no_tile) ? GetRange() : GetTileRange(t);
			SizeT nrRows = range.empty() ? 0 : 1;
			return I64Rect(shp2dms_order<Int64>(range.first, 0), shp2dms_order<Int64>(range.second, nrRows));
		}
		else
		{
			return ThrowingConvert<I64Rect>((t == no_tile) ? GetRange() : GetTileRange(t));
		}
	}
	V GetTileValue(tile_id t, tile_offset localIndex) const
	{
		assert(t != no_tile);
		return Range_GetValue_checked(GetTileRange(t), localIndex);
	}
	row_id  GetFirstRowIndex(tile_id t) const override
	{
		return GetIndexForValue(this->GetTileValue(t, 0));
	}
	row_id  GetRowIndex(tile_id t, tile_offset tileOffset) const override
	{
		return GetIndexForValue(this->GetTileValue(t, tileOffset));
	}

	tile_offset GetTileSize(tile_id t) const override
	{
		return Cardinality(GetTileRange(t));
	}
	bool IsFirstValueZero() const 
	{ 
		if constexpr (dimension_of_v<V> == 2)
			return false;
		else
			return m_Range.first == 0;
	}

	range_type m_Range;
};

//----------------------------------------------------------------------

struct simple_range_provider {
	template <typename T> struct apply {
		using type = SimpleRangeData<T>;
	};
	template <typename T> struct apply_ptr {
		using type = SharedPtr<const SimpleRangeData<T>>;
	};
};

struct small_range_provider {
	template <typename T> struct apply {
		using type = SmallRangeData<T>;
	};
	template <typename T> struct apply_ptr {
		using type = SharedPtr<const SmallRangeData<T>>;
	};
};

struct tiled_range_provider {
	template <typename T> struct apply {
		using type = TiledRangeData<T>;
	};
	template <typename T> struct apply_ptr {
		using type = SharedPtr<const TiledRangeData<T>>;
	};
};

struct fixed_range_provider {
	template <typename T> struct apply { using type = Void; };
	template <typename T> struct apply_ptr { using type = Void; };

	template <> struct apply<Void> { using type = FixedRange<0>; };
	template <> struct apply_ptr<Void> { using type = Void; };

	template <bit_size_t N> struct apply<bit_value<N>> { using type = FixedRange<N>;  };
	template <bit_size_t N> struct apply_ptr<bit_value<N>> { using type = Void; };
};

struct void_provider {
	template <typename T> struct apply { using type = Void; };
	template <typename T> struct apply_ptr { using type = Void; };
};

template<typename T>
const bool has_simple_range_v = std::is_floating_point_v< scalar_of_t<T> >;

template<typename T>
const bool has_small_range_v = (sizeof(T) <= 2);

template<typename T>
using domain_range_provider = std::conditional_t<has_small_range_v<T>, small_range_provider, tiled_range_provider>;

template<typename T>
using range_provider = std::conditional_t<has_simple_range_v<T>, simple_range_provider, domain_range_provider<T>>;

template<typename T>
using range_or_void_provider = std::conditional_t<has_var_range_field_v<T>, range_provider<T>, fixed_range_provider>;

template<typename T>
using domain_range_data = typename domain_range_provider<T>::template apply<T>::type;

template<typename T>
using range_data = typename range_provider<T>::template apply<T>::type;

template<typename T>
using range_or_void_data = typename range_or_void_provider<T>::template apply<T>::type;

template<typename T>
using range_data_ptr_or_void = typename range_or_void_provider<T>::template apply_ptr<T>::type;

template <typename V>
struct MaxRangeData : TiledRangeData<V>
{
	MaxRangeData()
		: domain_range_data<V>(Range<V>(MinValue<V>(), MaxValue<V>()))
	{}

	tile_type_id GetTileTypeID() const { return tile_type_id::max; }

	tile_id GetNrTiles() const override { return 0; }
	tile_offset GetTileSize(tile_id t) const override { throwIllegalAbstract(MG_POS, "MaxRangeData::GetTileSize"); }
	tile_offset GetMaxTileSize() const override { throwIllegalAbstract(MG_POS, "MaxRangeData::GetMaxTileSize"); }
	tile_loc GetTiledLocation(row_id index) const override { throwIllegalAbstract(MG_POS, "MaxRangeData::GetTileLocation"); }
	SizeT GetNrMemPages(UInt8 log2BytesPerElem) const override { throwIllegalAbstract(MG_POS, "MaxRangeData::GetNrMemPages"); }
	SizeT GetMemPageIndex(UInt8 log2BytesPerElem, tile_id t) const override { throwIllegalAbstract(MG_POS, "MaxRangeData::GetMemPageIndex"); }

//	I64Rect GetRangeAsI64Rect() const override { return { {0, 0}, shp2dms_order(this->GetRangeSize(), row_id(1)) }; }
	I64Rect GetTileRangeAsI64Rect(tile_id t) const { throwIllegalAbstract(MG_POS, "MaxRangeData::GetTileRangeAsI64Rect"); }
	row_id GetFirstRowIndex(tile_id t) const override { throwIllegalAbstract(MG_POS, "MaxRangeData::GetFirstRowIndex"); }
	row_id GetRowIndex(tile_id t, tile_offset localIndex) const override { throwIllegalAbstract(MG_POS, "MaxRangeData::GetRowIndex"); }

	// range_t(dependent on T) specific functions, non virtual
	auto GetTileRange(tile_id t) const -> Range<V> override { throwIllegalAbstract(MG_POS, "MaxRangeData::GetTileRange"); }
	auto GetTilingExtent()       const -> tile_extent_t<V> override { throwIllegalAbstract(MG_POS, "MaxRangeData::GetTilingRange"); }

	row_id GetElemCount() const { throwIllegalAbstract(MG_POS, "MaxRangeData::GetElemCount"); }
	bool IsFirstValueZero() const { throwIllegalAbstract(MG_POS, "MaxRangeData::IsFirstValueZero"); }

	auto GetAsLispRef(LispPtr base, bool asCategorical) const->LispRef override { return base; }
};

//----------------------------------------------------------------------
// Conversions for CountablePoints (SPoints and IPoints)
//----------------------------------------------------------------------

template <typename T>
struct FixedRangeConverter {
	FixedRangeConverter(const range_data_ptr_or_void<T>& src) {}

	template <typename I> T GetValue(I i) const { return Convert<T>(i); }
	template <typename I> I GetScalar(T v) const { return Convert<I>(v); }
};

template <typename T>
struct CountableVarRangeConverter
{
	CountableVarRangeConverter(const range_data_ptr_or_void<T>& src) : m_Range(src->GetRange()) {}

	template <typename I> T GetValue(I i) const { return Range_GetValue_checked(m_Range, Convert<row_id>(i)); }
	template <typename I> I GetScalar(const Point<T>& v) const { return Convert<I>(Range_GetIndex_checked(m_Range, v)); }
private:
	Range<T> m_Range;
};

template <typename T>
struct CountableVarRangeConverter<Point<T>> {
	CountableVarRangeConverter(const range_data_ptr_or_void<Point<T>>& src) : m_Range(src->GetRange()) {}

	template <typename I> Point<T> GetValue(I i) const { return Range_GetValue_checked(m_Range, Convert<row_id>(i)); }
	template <typename I> I GetScalar(const Point<T>& v) const { return Convert<I>(Range_GetIndex_checked(m_Range, v)); }
private:
	Range<Point<T> > m_Range;
};

template <typename T>
struct fixed_range_converter_provider {
	template <typename T> struct apply {
		using type = FixedRangeConverter<T>;
	};
};

template <typename T>
struct countable_range_converter_provider {
	template <typename T> struct apply {
		using type = CountableVarRangeConverter<T>;
	};
};

template <typename T>
using countable_value_converter_provider = std::conditional_t<has_var_range_v<T>, countable_range_converter_provider<T>, fixed_range_converter_provider <T >>;

template <typename T>
using countable_point_converter_provider = std::conditional_t<(dimension_of_v<T> == 2), countable_range_converter_provider<T>, fixed_range_converter_provider <T >>;

template <typename T>
using CountableValueConverter = typename countable_value_converter_provider<T> ::template apply<T>::type;

template <typename T>
using CountablePointConverter = typename countable_point_converter_provider<T> ::template apply<T>::type;


#endif // __TIC_TILERANGEDATA_H
