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

#include "geo/IsNotUndef.h"
#include "mci/CompositeCast.h"
#include "mci/ValueClass.h"
#include "set/DataCompare.h"
#include "utl/TypeListOper.h"
#include "RtcTypeLists.h"
#include "ASync.h"

#include "DataArray.h"
#include "DataItemClass.h"
#include "TileChannel.h"
#include "Unit.h"
#include "UnitClass.h"


template <typename V> const UInt32 BUFFER_SIZE = 4096 / sizeof(V);

template <typename V>
std::vector<V> GetUniqueValuesDirect(typename DataArray<V>::locked_cseq_t seq, tile_offset index, tile_offset size)
{
	// PRECONDITIONS
	assert(size <= BUFFER_SIZE<V>);
	assert(size > 0);
	assert(index <= seq.size());
	assert(index + size <= seq.size());

	V buffer[BUFFER_SIZE<V>]; V* bufferCursor = buffer;
	//	fast_copy(seq.begin() + index, seq.begin() + index + size, buffer);

	//	copy only values that are defined and not equal to their predecessor
	auto i = seq.begin() + index, e = i + size;

redo:
		if (IsNotUndef(*i))
			*bufferCursor++ = *i;
		auto pi = i;
		while (++i != e)
		{
			if (*i != *pi)
				goto redo;
		}
	assert(i == e);

	std::sort(buffer, bufferCursor);

	auto lastValuePtr = std::unique(buffer, bufferCursor);
	return std::vector<V>( buffer, lastValuePtr );
}

template <typename Iter, typename Pred>
_CONSTEXPR20 auto set_union_by_move(Iter first1, Iter last1, Iter first2, Iter last2, Iter dest, Pred pred) -> Iter
{
	for (; first1 != last1 && first2 != last2; ++dest) {
		if (pred(*first1, *first2)) { // copy first
			*dest = std::move(*first1);
			++first1;
		}
		else if (pred(*first2, *first1)) { // copy second
			*dest = std::move(*first2);
			++first2;
		}
		else { // advance both
			*dest = std::move(*first1);
			++first1;
			++first2;
		}
	}

	dest = fast_move(first1, last1, dest);
	dest = fast_move(first2, last2, dest);
	return dest;
}

template <typename V>
std::vector<V> MergeToLeft(std::vector<V> left, std::vector<V> right)
{
	std::vector<V> result;
	result.resize(left.size() + right.size());

	if (!result.empty())
	{
		auto actualEnd = set_union_by_move(right.begin(), right.end(), left.begin(), left.end(), result.begin(), std::less<void>());
		result.erase(actualEnd, result.end());
	}
	return result;
}

#include <future>

template <typename V>
std::vector<V> GetTileUniqueValues(typename DataArray<V>::locked_cseq_t tileData, tile_offset index, tile_offset size)
{
	if (size <= BUFFER_SIZE<V>)
		return GetUniqueValuesDirect<V>(tileData, index, size);

	std::vector<V> result;
	tile_offset m = size / 2;

	std::future<std::vector<V>> firstHalf = throttled_async([&tileData, index, m]() {
		return GetTileUniqueValues<V>(tileData, index, m);
		});

	auto secondHalf = GetTileUniqueValues<V>(tileData, index + m, size - m);
	return MergeToLeft<V>(std::move(firstHalf.get()), std::move(secondHalf));
}

template <typename V>
std::vector<V> GetUniqueWallValues(const DataArray<V>* ado, tile_id t, tile_id nrTiles)
{
	dms_assert(nrTiles >= 1); // PRECONDITION
	if (nrTiles == 1)
	{
		auto tileSize = ado->GetTiledRangeData()->GetTileSize(t);
		return GetTileUniqueValues<V>(ado->GetTile(t), 0, tileSize);
	}

	tile_id m = nrTiles / 2;
	dms_assert(m >= 1);

	std::future<std::vector<V>> firstHalf = throttled_async([ado, t, m]() {
		return GetUniqueWallValues<V>(ado, t, m);
		});

	auto secondHalf = GetUniqueWallValues<V>(ado, t + m, nrTiles - m);
	return MergeToLeft<V>(firstHalf.get(), secondHalf);
}


template<fixed_elem V>
std::vector<V> GetUniqueValues(const AbstrDataItem* adi)
{
	dms_assert(adi && adi->GetInterestCount());

	const DataArray<V>* ado = const_array_cast<V>(adi);

	SizeT count = ado->GetTiledRangeData()->GetElemCount();
	if (count)
	{
		tile_id tn = ado->GetTiledRangeData()->GetNrTiles();
		if (tn)
			return GetUniqueWallValues<V>(ado, 0, tn);
	}
	return {};
}

template<sequence_or_string V>
auto GetUniqueValues(const AbstrDataItem* adi) -> auto
{
	std::set<V, std::less<void> > values;

	const DataArray<V>* ado = const_array_cast<V>(adi);

	for (tile_id t = 0, tn = ado->GetTiledRangeData()->GetNrTiles(); t != tn; ++t)
	{
		auto tileData = ado->GetTile(t);
		auto i = tileData.begin(), e = tileData.end();

		if (i != e)
		{
		redo:
			if (IsDefined(*i))
			{
				auto sequenceRef = *i;
				auto insertPos = values.lower_bound(sequenceRef);
				if (insertPos == values.end() || values.key_comp()(sequenceRef, *insertPos))
					values.insert(insertPos, *i);
			}
			auto pi = i;
			while (++i != e)
				if (!(*i == *pi))
					goto redo;
		}
		dms_assert(i == e);
	}
	return values;
}


// *****************************************************************************
//                         Unique
// *****************************************************************************

CommonOperGroup cog_unique ("unique", oper_policy::dynamic_result_class);
static TokenID s_Values = GetTokenID_st("Values");

class AbstrUniqueOperator : public UnaryOperator
{
public:
	// Override Operator
	AbstrUniqueOperator(ClassCPtr argCls)
		: UnaryOperator(&cog_unique, AbstrUnit::GetStaticClass(), argCls) 
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 1);

		const AbstrDataItem*  arg1 = debug_cast<const AbstrDataItem*>(args[0]);

		dms_assert(arg1);
		const AbstrUnit*  arg1Values   = arg1->GetAbstrValuesUnit();
		const ValueClass* vc           = arg1->GetAbstrDomainUnit()->GetUnitClass()->GetValueType();
		const UnitClass*  resDomainCls = UnitClass::Find(vc->GetCrdClass());

		AbstrUnit* res = resDomainCls->CreateResultUnit(resultHolder);
		dms_assert(res);
		resultHolder = res;

		AbstrDataItem* resSub = CreateDataItem(res, s_Values, res, arg1Values, arg1->GetValueComposition() );
		MG_PRECONDITION(resSub);
		
		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1);
			Calculate(res, resSub, arg1);
		}
		return true;
	}
	virtual void Calculate(AbstrUnit* res, AbstrDataItem* resSub, const AbstrDataItem* arg1) const =0;
};

template <typename V>
class UniqueOperator : public AbstrUniqueOperator
{
	typedef DataArray<V>      ArgType;
	typedef AbstrUnit         ResultType;
	typedef DataArray<V>      ResultSubType;

public:
	// Override Operator
	UniqueOperator()
		:	AbstrUniqueOperator(ArgType::GetStaticClass())
	{}

	void Calculate(AbstrUnit* res, AbstrDataItem* resSub, const AbstrDataItem* arg1A) const override
	{
		auto values = GetUniqueValues<V>(arg1A);

		res->SetCount(values.size());
		
		locked_tile_write_channel<V> resWriter(resSub);
		resWriter.Write(values.begin(), values.end());
		dms_assert(resWriter.IsEndOfChannel());
		resWriter.Commit();
	}
};

// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

namespace 
{
	tl_oper::inst_tuple<typelists::value_elements, UniqueOperator<_> > uniqueOperators;
} // end anonymous namespace



