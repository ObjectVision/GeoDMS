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

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

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
#include "UnitProcessor.h"

#include "rlookup.h"

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
auto set_union_by_move(Iter first1, Iter last1, Iter first2, Iter last2, Iter dest, Pred pred) -> Iter
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
void GetUniqueValues(AbstrUnit* res, AbstrDataItem* resSub, const AbstrDataItem* adi)
{
	dms_assert(adi && adi->GetInterestCount());

	const DataArray<V>* ado = const_array_cast<V>(adi);
	std::vector<V> values;
	SizeT count = ado->GetTiledRangeData()->GetElemCount();
	if (count)
	{
		tile_id tn = ado->GetTiledRangeData()->GetNrTiles();
		if (tn)
			values = GetUniqueWallValues<V>(ado, 0, tn);
	}

	res->SetCount(values.size());

	locked_tile_write_channel<V> resWriter(resSub);
	resWriter.Write(values.begin(), values.end());
	dms_assert(resWriter.IsEndOfChannel());
	resWriter.Commit();
}

template <typename Iter, typename Pred>
Iter make_strict_monotonous(Iter first, Iter last, Pred pred)
{
	if (first == last)
		return last;
	Iter result = first;
	while (++first != last && pred(*result, *first))
		++result;
	if (first == last)
		return last;
	assert(result != first);
	while (++first != last)
		if (pred(*result, *first))
			*++result = std::move(*first);

	return ++result;
}

template<sequence_or_string V>
void GetUniqueValues(AbstrUnit* res, AbstrDataItem* resSub, const AbstrDataItem* adi)
{
	auto allValues = const_array_cast<V>(adi)->GetDataRead();

	using ConstDataIter = DataArrayBase<V>::const_iterator;
	visit<typelists::domain_elements>(adi->GetAbstrDomainUnit(), [res, resSub, allValues]<typename E>(const Unit<E>*arg2Domain)
	{
		using index_type = typename cardinality_type<E>::type;
		std::vector<index_type> index;
		make_index(index, allValues.size(), allValues.begin());
		auto indexEnd = make_strict_monotonous(index.begin(), index.end(), IndexCompareOper<ConstDataIter, index_type>(allValues.begin()));
		index.erase(indexEnd, index.end());

		SizeT nrUndefined = 0;
		for (auto i : index)
			if (!IsDefined(allValues[i]))
				++nrUndefined;

		res->SetCount(index.size() - nrUndefined);

		locked_tile_write_channel<V> resWriter(resSub);
		for (auto i : index)
			if (IsDefined(allValues[i]))	
				resWriter.Write(allValues[i]);
		assert(resWriter.IsEndOfChannel());
		resWriter.Commit();
	});
}


// *****************************************************************************
//                         Unique
// *****************************************************************************

CommonOperGroup cog_unique("unique", oper_policy::dynamic_result_class);
CommonOperGroup cog_unique64("unique_uint64", oper_policy::dynamic_result_class);
CommonOperGroup cog_unique32("unique_uint32", oper_policy::dynamic_result_class);
CommonOperGroup cog_unique16("unique_uint16", oper_policy::dynamic_result_class);
CommonOperGroup cog_unique08("unique_uint8", oper_policy::dynamic_result_class);

static TokenID s_Values = GetTokenID_st("Values");

class AbstrUniqueOperator : public UnaryOperator
{
public:
	// Override Operator
	AbstrUniqueOperator(ClassCPtr argCls, AbstrOperGroup& og, const UnitClass* resDomainCls)
		: UnaryOperator(&og, AbstrUnit::GetStaticClass(), argCls) 
		, m_ResDomainClass(resDomainCls)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 1);

		const AbstrDataItem*  arg1 = debug_cast<const AbstrDataItem*>(args[0]);

		assert(arg1);
		const AbstrUnit* arg1Values   = arg1->GetAbstrValuesUnit();
		const UnitClass* resDomainCls = m_ResDomainClass;
		if (!resDomainCls)
		{
			const ValueClass* vc = arg1->GetAbstrDomainUnit()->GetUnitClass()->GetValueType();
			resDomainCls = UnitClass::Find(vc->GetCrdClass());
		}

		AbstrUnit* res = resDomainCls->CreateResultUnit(resultHolder);
		assert(res);
		res->SetTSF(TSF_Categorical);
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

	const UnitClass* m_ResDomainClass = nullptr;
};

template <typename V>
class UniqueOperator : public AbstrUniqueOperator
{
	typedef DataArray<V>      ArgType;
	typedef AbstrUnit         ResultType;
	typedef DataArray<V>      ResultSubType;

public:
	// Override Operator
	UniqueOperator(AbstrOperGroup& og, const UnitClass* resDomainCls)
		:	AbstrUniqueOperator(ArgType::GetStaticClass(), og, resDomainCls)
	{}

	void Calculate(AbstrUnit* res, AbstrDataItem* resSub, const AbstrDataItem* arg1A) const override
	{
		GetUniqueValues<V>(res, resSub, arg1A);
	}
};

// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

namespace 
{
	tl_oper::inst_tuple_templ<typelists::value_elements, UniqueOperator, AbstrOperGroup&, const UnitClass*>
		uniqueOperatorsXX(cog_unique, nullptr)
	, uniqueOperators64(cog_unique64, Unit<UInt64>::GetStaticClass())
	, uniqueOperators32(cog_unique32, Unit<UInt32>::GetStaticClass())
	, uniqueOperators16(cog_unique16, Unit<UInt16>::GetStaticClass())
	, uniqueOperators08(cog_unique08, Unit<UInt8>::GetStaticClass());
} // end anonymous namespace



