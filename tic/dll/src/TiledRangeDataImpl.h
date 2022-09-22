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

#if !defined(__TIC_SEGMENTATIONINFO_H)
#define __TIC_SEGMENTATIONINFO_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "TicBase.h"

#include "TiledRangeData.h"

#include "geo/Point.h"
#include "geo/Range.h"

#include "LispList.h"

#include "LispTreeType.h"

template <typename V>
tile_extent_t<V> default_tile_size() {
	if constexpr (is_numeric_v<V>)
		return 1 << log2_default_segment_size;
	else
		return WPoint(1 << log2_default_tile_size, 1 << log2_default_tile_size);
}

//----------------------------------------------------------------------

template <typename V>
struct DefaultTileRangeDataBase : TiledRangeData<V>
{
	DefaultTileRangeDataBase() = default;
	DefaultTileRangeDataBase(const Range<V>& range) : TiledRangeData<V>(range) {}

	LispRef GetAsLispRef(LispPtr base) const override
	{
		return AsLispRef(this->m_Range, base);
	}

	static tile_extent_t<V> tile_extent() { return default_tile_size<V>(); }
};

template <typename V>
struct RegularTileRangeDataBase : TiledRangeData<V>
{
	RegularTileRangeDataBase() = default;
	RegularTileRangeDataBase(const Range<V>& range, tile_extent_t<V> tileExtent) : TiledRangeData<V>(range)
		, m_TileExtent(tileExtent) {}

	tile_extent_t<V> tile_extent() const { return m_TileExtent; }

	tile_type_id GetTileTypeID() const override { return tile_type_id::regular; }

	void Load(BinaryInpStream& pis) override;
	void Save(BinaryOutStream& pis) const override;

	LispRef GetAsLispRef(LispPtr base) const override
	{
		auto result = List(LispRef(token::TiledUnit)
			,	slConvertedLispExpr(AsLispRef(m_TileExtent)
				,	AsLispRef(this->m_Range, base)
				)
			);

#if defined(MG_DEBUG_LISP_TREE)
		reportD(SeverityTypeID::ST_MinorTrace, AsString(result).c_str());
#endif
		return result;
	}

	tile_extent_t<V> m_TileExtent;
};

template <typename Base>
struct RegularAdapter: Base
{
//	using Base::Base; // insert ctors of Base.
	template <typename ...Args>
	RegularAdapter(Args&& ...args)
	:	Base(std::forward<Args>(args)...)
	{
		CalcTilingExtent();
	}

	using value_type = typename Base::value_type;

	auto tiling_extent() const { return m_TilingExtent;  };

	auto GetTileRange(tile_id t) const->Range<value_type> override;
	tile_loc GetTiledLocationForValue(value_type v) const;

	tile_loc GetTiledLocation(row_id index) const override;
	tile_id GetNrTiles() const  override;

	void CalcTilingExtent() override;

	tile_offset GetMaxTileSize() const override
	{
		auto maxExtent = this->tile_extent();
		auto rangeSize = Size(this->GetRange());
		MakeLowerBound(maxExtent, Convert<tile_extent_t<value_type>>( rangeSize ));
		return Cardinality(maxExtent);
	}


private:
	tile_extent_t<value_type> m_TilingExtent;
};


template <typename V> using DefaultTileRangeData = RegularAdapter< DefaultTileRangeDataBase<V> >;
template <typename V> using RegularTileRangeData = RegularAdapter< RegularTileRangeDataBase<V> >;

template <typename V>
struct IrregularTileRangeData : TiledRangeData<V>
{
	IrregularTileRangeData() = default;
	IrregularTileRangeData(std::vector<Range<V>> ranges) 
		: TiledRangeData<V>(Range<V>(ranges.begin(), ranges.end(), true, false))
		, m_Ranges(std::move(ranges)) 
	{}

	Range<V> GetTileRange(tile_id t) const override
	{
		dms_assert(t < m_Ranges.size());
		return m_Ranges[t];
	}

	tile_type_id GetTileTypeID() const override { return tile_type_id::irregular; }

	void Load(BinaryInpStream& pis) override;
	void Save(BinaryOutStream& pis) const override;

	LispRef GetAsLispRef(LispPtr base) const override
	{
		std::vector<V> boundValues; boundValues.reserve(m_Ranges.size());
		for (const auto& r : m_Ranges)
			boundValues.push_back(r.first);
		auto lb = AsLispRef(boundValues);
		boundValues.clear();
		for (const auto& r : m_Ranges)
			boundValues.push_back(r.second);
		auto ub = AsLispRef(boundValues);

		static auto tiledUnitSymb = LispRef(token::TiledUnit);
		static auto tokenSymb = LispRef(token::convert);
		return 
			List(tiledUnitSymb
			,	List(tokenSymb, lb, base)
			,	List(tokenSymb, ub, base)
			);
	}

	tile_loc GetTiledLocationForValue(V v) const
	{
		dms_assert(IsIncluding(this->m_Range, v));
		for (const auto& tileRange: m_Ranges)
		{
			if (IsIncluding(tileRange, v))
			{
				return tile_loc(&tileRange - begin_ptr(m_Ranges), Range_GetIndex_naked(tileRange, v));
			}
		}
		return tile_loc(no_tile, UNDEFINED_VALUE(tile_offset));
	}

	tile_loc GetTiledLocation(row_id index) const override
	{
		dms_assert(index < Cardinality(this->m_Range));

		V v = Range_GetValue_naked(this->m_Range, index);
		return GetTiledLocationForValue(v);
	}
	tile_loc GetTiledLocation(row_id index, tile_id prevT) const override
	{
		V v = Range_GetValue_naked(this->m_Range, index);
		if (prevT < m_Ranges.size())
		{
			const auto& prevTileRange = m_Ranges[prevT];
			if (IsIncluding(prevTileRange, v))
			{
				return tile_loc(prevT, Range_GetIndex_naked(prevTileRange, v));
			}
		}
		return GetTiledLocationForValue(v);
	}

	tile_id GetNrTiles() const override
	{
		return m_Ranges.size();
	}
	bool IsCovered() const override
	{
		return false; // pessimistic base impl
	}
	tile_offset GetMaxTileSize() const override
	{
		tile_offset result = 0;
		for (auto& tileRange : m_Ranges)
			MakeMax(result, Cardinality(tileRange));
		return result;
	}
	row_id GetDataSize() const override
	{
		row_id nrRows = 0;
		for (tile_id t = 0, tn = GetNrTiles(); t != tn; ++t)
			nrRows += this->GetTileSize(t);
		return nrRows;
	}

	std::vector<Range<V>> m_Ranges;
};


#endif // __TIC_SEGMENTATIONINFO_H
