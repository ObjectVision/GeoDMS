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
#include "geo/SeqVector.h"
#include "mci/CompositeCast.h"
#include "mci/ValueClass.h"
#include "set/DataCompare.h"
#include "utl/TypeListOper.h"
#include "RtcTypeLists.h"
#include "ASync.h"

#include "DataArray.h"
#include "DataItemClass.h"
#include "Unit.h"
#include "UnitClass.h"


template <typename V> const UInt32 BUFFER_SIZE = 4096 / sizeof(V);

template <typename V>
std::vector<V> GetUniqueValuesDirect(typename DataArray<V>::locked_cseq_t seq, SizeT index, UInt32 size)
{
	dms_assert(size <= BUFFER_SIZE<V>);
	dms_assert(size > 0);

	V buffer[BUFFER_SIZE<V>];
	dms_assert(index + size <= seq.size());
	fast_copy(seq.begin() + index, seq.begin() + index + size, buffer);

	DataCompare<V> comp;
	std::sort(buffer, buffer + size, comp);

	UInt32 nrNulls = 0;

	while (true)
	{
		if (nrNulls == size)
			return {};
		if (IsNotUndef(buffer[nrNulls]))
			break;
		++nrNulls;
	}
	auto firstValuePtr = buffer + nrNulls;
	auto lastValuePtr = std::unique(firstValuePtr, buffer+size);
	return std::vector<V>( firstValuePtr, lastValuePtr );
}

template <class _InIt1, class _InIt2, class _OutIt, class _Pr>
_CONSTEXPR20 _OutIt set_union_by_move(_InIt1 _First1, _InIt1 _Last1, _InIt2 _First2, _InIt2 _Last2, _OutIt _Dest, _Pr _Pred) {
	// OR sets [_First1, _Last1) and [_First2, _Last2)
	_Adl_verify_range(_First1, _Last1);
	_Adl_verify_range(_First2, _Last2);
	auto _UFirst1 = _Get_unwrapped(_First1);
	const auto _ULast1 = _Get_unwrapped(_Last1);
	auto _UFirst2 = _Get_unwrapped(_First2);
	const auto _ULast2 = _Get_unwrapped(_Last2);
	_DEBUG_ORDER_SET_UNWRAPPED(_InIt2, _UFirst1, _ULast1, _Pred);
	_DEBUG_ORDER_SET_UNWRAPPED(_InIt1, _UFirst2, _ULast2, _Pred);
	auto _UDest = _Get_unwrapped_unverified(_Dest);
	for (; _UFirst1 != _ULast1 && _UFirst2 != _ULast2; ++_UDest) {
		if (_DEBUG_LT_PRED(_Pred, *_UFirst1, *_UFirst2)) { // copy first
			*_UDest = std::move(*_UFirst1);
			++_UFirst1;
		}
		else if (_Pred(*_UFirst2, *_UFirst1)) { // copy second
			*_UDest = std::move(*_UFirst2);
			++_UFirst2;
		}
		else { // advance both
			*_UDest = std::move(*_UFirst1);
			++_UFirst1;
			++_UFirst2;
		}
	}

	_UDest = fast_move(_UFirst1, _ULast1, _UDest);
	_Seek_wrapped(_Dest, fast_move(_UFirst2, _ULast2, _UDest));
	return _Dest;
}

template <typename V>
std::vector<V> MergeToLeft(std::vector<V> left, std::vector<V> right)
{
	std::vector<V> result;
	result.resize(left.size() + right.size());

	if (!result.empty())
	{
		auto actualEnd = set_union_by_move(right.begin(), right.end(), left.begin(), left.end(), result.begin(), std::less<V>());
		result.erase(actualEnd, result.end());
	}
	return result;
}

#include <future>

template <typename V>
std::vector<V> GetTileUniqueValues(typename DataArray<V>::locked_cseq_t tileData, SizeT index, SizeT size)
{
	if (size <= BUFFER_SIZE<V>)
		return GetUniqueValuesDirect<V>(tileData, index, size);

	std::vector<V> result;
	SizeT m = size / 2;
//	std::vector<V> firstHalf, secondHalf;

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
//		ReadableTileLock tileLock(ado, t);
		SizeT tileSize = ado->GetTiledRangeData()->GetTileSize(t);
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


template<typename V> 
std::vector<V> GetUniqueValues(const AbstrDataItem* adi)
{
	dms_assert(adi && adi->GetInterestCount());

	DataReadLock lck(adi);

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

	// TODO, std::set vervangen door make_index 
	void Calculate(AbstrUnit* res, AbstrDataItem* resSub, const AbstrDataItem* arg1A) const override
	{
		typedef std::vector<V> ValueSet;
		ValueSet values = GetUniqueValues<V>(arg1A);

		res->SetCount(values.size());
		
		DataWriteLock resSubLock(resSub);
		ResultSubType* resultSub = mutable_array_cast<V>(resSubLock);
		dms_assert(resultSub);

		auto resSubData = resultSub->GetDataWrite();
		dms_assert(resSubData.size() == values.size());

		auto
			vi = values.begin(),
			ve = values.end();
		auto ri = resSubData.begin();
		for (; vi!=ve; ++ri, ++vi)
   			*ri = *vi;

		resSubLock.Commit();
	}
};

// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

namespace 
{
	tl_oper::inst_tuple<typelists::value_elements, UniqueOperator<_> > uniqueOperators;
} // end anonymous namespace



