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

#include "geo/Conversions.h"
#include "geo/IsNotUndef.h"
#include "mci/CompositeCast.h"
#include "set/VectorFunc.h"

#include "DataItemClass.h"
#include "Param.h"
#include "TreeItemClass.h"

#include "OperAccUni.h"
#include "OperAccBin.h"
#include "OperRelUni.h"
#include "UnitCreators.h"
#include "IndexGetterCreator.h"

// *****************************************************************************
//											Modus Helper funcs
// *****************************************************************************

template <typename ACC, typename CFI>
SizeT argmax(CFI bufferB, UInt32 lastVi)
{
	SizeT maxVi = UNDEFINED_VALUE(SizeT);
	ACC maxC = 0; // MIN_VALUE(ACC);
	for (UInt32 vi = 0; vi != lastVi; ++vi)
	{
		auto c = *bufferB++;
		if (c > maxC) { maxC = c; maxVi = vi; }
	}
	return maxVi;
}

bool OnlyDefinedCheckRequired(const AbstrDataItem* adi)
{
	DataCheckMode dcm = adi->GetCheckMode();
	return !(dcm & DCM_CheckDefined);
}
/* 
template <typename V>
typename Unit<V>::range_t 
GetRange(const DataArray<V>* da)
{
	return da->GetValueRangeData()->GetRange();
}
*/
template <typename V>
typename Unit<V>::range_t
GetRange(const AbstrDataItem* adi)
{
	return debug_cast<const Unit<V>*>(adi->GetAbstrValuesUnit())->GetRange();
}

// *****************************************************************************
//											ModusTot
// *****************************************************************************

// assume v >> n; time complexity: n*log(min(v, n))
template<typename V>
void ModusTotBySet(const AbstrDataItem* valuesItem, typename sequence_traits<V>::reference resData)
{
	std::map<V, SizeT> counters;

	auto values_fta = (DataReadLock(valuesItem), GetFutureTileArray(const_array_cast<V>(valuesItem)));
	for (tile_id t =0, tn = valuesItem->GetAbstrDomainUnit()->GetNrTiles(); t!=tn; ++t)
	{
		auto valuesLock = values_fta[t]->GetTile(); values_fta[t] = nullptr;
		auto valuesIter = valuesLock.begin(),
		     valuesEnd  = valuesLock.end();

		for (; valuesIter != valuesEnd; ++valuesIter)
			if (IsDefined(*valuesIter))
				++counters[*valuesIter];
	}

	SizeT    maxC    = 0;
	resData = UNDEFINED_VALUE(V);
	for (auto i = counters.begin(), e = counters.end(); i!=e; ++i)
	{
		if (i->second > maxC)
		{
			maxC    = i->second;
			resData = i->first;
		}
	}
}

// assume v >> n; time complexity: n*log(min(v, n))
template<typename V>
void ModusTotByIndex(const AbstrDataItem* valuesItem, typename sequence_traits<V>::reference resData)
{
	DataReadLock lock(valuesItem);
	auto valuesLock  = const_array_cast<V>(valuesItem)->GetLockedDataRead();
	auto valuesBegin = valuesLock.begin(),
	     valuesEnd   = valuesLock.end();

	SizeT n = valuesEnd - valuesBegin;
	OwningPtrSizedArray<SizeT> index(n, dont_initialize MG_DEBUG_ALLOCATOR_SRC("ModusTotByIndex: index"));
	auto i = index.begin(), e = index.end(); assert(e - i == n);
	make_index_in_existing_span(i, e, valuesBegin);

	SizeT maxC = 0;
	resData = UNDEFINED_VALUE(V);
	while (i != e)
	{
		decltype(valuesBegin) vPtr = valuesBegin + *i; V v = *vPtr;
		if (IsDefined(v))
		{
			SizeT c = 1;
			while (++i != e && valuesBegin[*i] == v)
				++c;
			dms_assert(c > 0);
			if (c > maxC)
			{
				maxC = c;
				resData = v;
			}
		}
		else
			while (++i != e && valuesBegin[*i] == v)
				;
	};
}

template<typename V>
void ModusTotByIndexOrSet(
	const AbstrDataItem* valuesItem,
	typename sequence_traits<V>::reference resData)
{
	if (valuesItem->GetAbstrDomainUnit()->IsCurrTiled())
		ModusTotBySet  <V>(valuesItem, resData);
	else
		ModusTotByIndex<V>(valuesItem, resData);
}


template<typename V>
void ModusTotByTable(
	const AbstrDataItem* valuesItem,
	typename sequence_traits<V>::reference resData, 
	typename Unit<V>::range_t valuesRange)
{
	SizeT vCount = Cardinality(valuesRange);
	std::vector<SizeT> buffer(vCount, 0);
	auto bufferB = buffer.begin();

	auto values_fta = (DataReadLock(valuesItem), GetFutureTileArray(const_array_cast<V>(valuesItem)));

	for (tile_id t =0, tn = valuesItem->GetAbstrDomainUnit()->GetNrTiles(); t!=tn; ++t)
	{
		auto valuesLock = values_fta[t]->GetTile(); values_fta[t] = nullptr;
		auto valuesIter  = valuesLock.begin(),
		     valuesEnd   = valuesLock.end();
		for (; valuesIter != valuesEnd; ++valuesIter)
		{
			if (IsDefined(*valuesIter))
			{
				UInt32 i = Range_GetIndex_naked(valuesRange, *valuesIter);
				dms_assert(i < vCount);
				++(bufferB[i]);
			}
		}
	}

	vCount = argmax<SizeT>(bufferB, vCount);
	if (IsDefined(vCount))
		resData = Range_GetValue_naked(valuesRange, vCount);
	else
		resData = UNDEFINED_OR_ZERO(V);
}

template<typename V>
void ModusTotDispatcher(
	const AbstrDataItem* valuesItem
,	typename sequence_traits<V>::reference resData
,	const inf_type_tag*)
{
	// NonCountable values; go for Set implementation
	ModusTotByIndexOrSet<V>(valuesItem, resData);
}

// make tradeoff between 
//      ModusPartTable: O(n+v*p)           processing with O(v*p) temp memory
//	and ModusPartSet:   O(n*log(min(n,v))) processing with O(t) temp memory with t <= min(n,v*p)
// when v is not countable(such as string, float), always choose the second method
//
// Note that ModusTotal is a special case of ModusPatial with p=1
//
// When memory condition doesnt favour Set: n >= v*p
// then Table time O(n+v*p) <= O(2n) < O(n*log(min(n,v))
// Thus, tradeof is made at v*p <= n.

template<typename V>template <typename V> using map_node_type = std::_Tree_node<std::pair<std::pair<SizeT, V>, SizeT>, void*>;
template <typename V> constexpr UInt32 map_node_type_size = sizeof(map_node_type<V>);

template <typename V> 
void ModusTotDispatcher(
	const AbstrDataItem* valuesItem
,	typename sequence_traits<V>::reference resData
,	const int_type_tag*)
{
	typename Unit<V>::range_t valuesRange = GetRange<V>( valuesItem );
	// Countable values; go for Table if sensible
	SizeT
		n = valuesItem->GetAbstrDomainUnit()->GetCount(),
		v = Cardinality(valuesRange);

	if	(	IsDefined(v)
		&&	(v / map_node_type_size<V> <= n / sizeof(V))
		&& OnlyDefinedCheckRequired(valuesItem) // memory condition v*p<=n, thus TableTime <= 2n.
		)
		ModusTotByTable<V>(
			valuesItem
		,	resData
		,	valuesRange
		);
	else
		ModusTotByIndexOrSet<V>(valuesItem, resData);
}

template<typename V>
void ModusTotDispatcher(
	const AbstrDataItem* valuesItem,
	typename sequence_traits<V>::reference resData,
	const bool_type_tag*)
{
	ModusTotByTable<V>(
		valuesItem
	,	resData
	,	GetRange<V>( valuesItem )
	);
}

// *****************************************************************************
//									ModusPart
// *****************************************************************************

// assume v >> n; time complexity: n*log(min(v, n))
template<typename V, typename OIV>
void ModusPartBySet(const AbstrDataItem* indicesItem, future_tile_array<V> values_fta, abstr_future_tile_array part_fta, OIV resBegin, SizeT pCount)  // countable dommain unit of result; P can be Void.
{
	assert(values_fta.size() == part_fta.size());

	using value_type = std::pair<SizeT, V>;
	std::map<value_type, SizeT> counters;

	for (tile_id t=0, tn= values_fta.size(); t!=tn; ++t)
	{
		auto valuesLock = values_fta[t]->GetTile(); values_fta[t] = nullptr;
		OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(indicesItem, part_fta[t]); part_fta[t] = nullptr;
		auto valuesIter  = valuesLock.begin(),
			 valuesEnd   = valuesLock.end();
		SizeT i=0;
		for (; valuesIter != valuesEnd; ++i, ++valuesIter)
			if (IsDefined(*valuesIter))
			{
				SizeT pi = indexGetter->Get(i);
				if (IsDefined(pi))
				{
					assert(pi < pCount);
					++counters[value_type(pi, *valuesIter)];
				}
			}
	}
	auto i = counters.begin(), e = counters.end();
	while (i != e)
	{
		SizeT pi = i->first.first;
		dms_assert(IsNotUndef(pi));
		dms_assert(pi < pCount);
		SizeT maxC = 0;
		do 
		{
			SizeT c = i->second;
			dms_assert( i->second>0);
			if ( i->second > maxC)
			{
				maxC         = i->second;
				resBegin[pi] = i->first.second;
			}
		}	while (++i != e && i->first.first == pi);
	}
}

// assume v >> n; time complexity: n*log(min(v, n))
template<typename V, typename OIV>
void ModusPartByIndex(const AbstrDataItem* indicesItem, typename DataArray<V>::locked_cseq_t values, abstr_future_tile* part_ft, OIV resBegin, SizeT pCount)
{
	auto valuesBegin = values.begin();
	auto valuesEnd   = values.end();

	OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(indicesItem, part_ft);

	SizeT n = valuesEnd - valuesBegin;

	OwningPtrSizedArray<SizeT> index(n, dont_initialize MG_DEBUG_ALLOCATOR_SRC("ModusPartByIndex: index"));
	auto i = index.begin(), e = index.end(); assert(e - i == n);
	make_indexP_in_existing_span(i, e, indexGetter, valuesBegin);

	while (i != e)
	{
		SizeT p = indexGetter->Get(*i);
		if (!IsDefined(p))
		{
			++i;
			continue;
		}
		else
		{
			dms_assert(p < pCount);
			SizeT maxC = 0;
			do 
			{
				decltype(valuesBegin) vPtr = valuesBegin + *i; V v = *vPtr;
				if (IsDefined(v))
				{
					SizeT c = 1;
					while (	++i != e &&	valuesBegin[*i] == v && indexGetter->Get(*i) == p )
						++c;
					dms_assert(c>0);
					if ( c > maxC)
					{
						maxC = c;
						resBegin[p] = v;
					}
				}
				else
				{
					while	(	++i != e 
							&&	valuesBegin[*i]   == v
							&&	indexGetter->Get(*i) == p 
							)
						;
				}
			}	while (i != e && indexGetter->Get(*i) == p);
		}
	}
}

template<typename V, typename OIV>
void ModusPartByIndexOrSet(const AbstrDataItem* indicesItem, future_tile_array<V> values_fta, abstr_future_tile_array part_fta, OIV resBegin, SizeT nrP)  // countable dommain unit of result; P can be Void.
{
	fast_fill(resBegin, resBegin+nrP, UNDEFINED_OR_ZERO(V));

	assert(values_fta.size() == part_fta.size());
	if (values_fta.size() != 1)
		ModusPartBySet  <V, OIV>(indicesItem, std::move(values_fta), std::move(part_fta), resBegin, nrP);
	else
		ModusPartByIndex<V, OIV>(indicesItem, values_fta[0]->GetTile(), part_fta[0], resBegin, nrP);
}


template<typename V, typename OIV>
void ModusPartByTable(const AbstrDataItem* indicesItem, future_tile_array<V> values_fta, abstr_future_tile_array part_afta
	, OIV resBegin, typename Unit<V>::range_t valuesRange, SizeT pCount)  // countable dommain unit of result; P can be Void.
{
	SizeT vCount = Cardinality(valuesRange);
	std::vector<SizeT> buffer(vCount*pCount, 0);
	auto bufferB = buffer.begin();

	for (tile_id t =0, tn = values_fta.size(); t != tn; ++t)
	{
		auto values = values_fta[t]->GetTile(); values_fta[t] = nullptr;
		auto valuesIter = values.begin(),
		     valuesEnd  = values.end();

		OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(indicesItem, part_afta[t]); part_afta[t] = nullptr;
		SizeT i=0;

		for (; valuesIter != valuesEnd; ++i, ++valuesIter)
		{
			UInt32 vi = Range_GetIndex_naked(valuesRange, *valuesIter);
			if (vi >= vCount)
				continue;
			SizeT pi = indexGetter->Get(i);
			if (pi < pCount)
				++(bufferB[pi * vCount + vi]);
		}
	}

	for (OIV resEnd = resBegin + pCount; resBegin != resEnd; ++resBegin)
	{
		SizeT i = argmax<UInt32>(bufferB, vCount);
		if (i < vCount)
			*resBegin = Range_GetValue_naked(valuesRange, i);
		else
			*resBegin = UNDEFINED_OR_ZERO(V);

		bufferB += vCount;
	}
	assert(bufferB == buffer.end());
}

// *****************************************************************************
//											WeightedModusTot
// *****************************************************************************

// assume v >> n; time complexity: n*log(min(v, n))
template<typename V>
void WeightedModusTotBySet(const AbstrDataItem* valuesItem, const AbstrDataItem* weightItem, typename sequence_traits<V>::reference resData)
{
	std::map<V, Float64> counters;

	for (tile_id t =0, tn = valuesItem->GetAbstrDomainUnit()->GetNrTiles(); t!=tn; ++t)
	{
		auto valuesLock  = const_array_cast<V>(valuesItem)->GetLockedDataRead(t);
		auto valuesIter  = valuesLock.begin(),
		     valuesEnd   = valuesLock.end();
		OwningPtr<AbstrValueGetter<Float64>> weightsGetter = WeightGetterCreator::Create(weightItem, t);

		SizeT weightsIter = 0;
		for (; valuesIter != valuesEnd; ++weightsIter, ++valuesIter)
			if (IsDefined(*valuesIter))
				counters[*valuesIter] += weightsGetter->Get(weightsIter);
	}

	Float64 maxC = 0;
	resData = UNDEFINED_VALUE(V);

	for (auto i = counters.begin(), e = counters.end(); i!=e; ++i)
	{
		if (i->second > maxC)
		{
			maxC    = i->second;
			resData = i->first;
		}
	}
}

// assume v >> n; time complexity: n*log(min(v, n))
template<typename V>
void WeightedModusTotByIndex(
	const AbstrDataItem* valuesItem,
	const AbstrDataItem* weightItem,
	typename sequence_traits<V>::reference resData)
{
	auto valuesLock    = const_array_cast<V>(valuesItem)->GetLockedDataRead();
	auto valuesBegin   = valuesLock.begin(),
	     valuesEnd     = valuesLock.end();
	OwningPtr<AbstrValueGetter<Float64>> weightsGetter = WeightGetterCreator::Create(weightItem);

	auto n = valuesEnd - valuesBegin;
	OwningPtrSizedArray<SizeT> index(n, dont_initialize MG_DEBUG_ALLOCATOR_SRC("WeightModusTotByIndex: index"));
	auto i = index.begin(), e = index.end(); assert(e - i == n);
	make_index_in_existing_span(i, e, valuesBegin);

	Float64 maxC = MIN_VALUE(Float64);
	resData = UNDEFINED_OR_ZERO(V);

	while (i != e)
	{
		decltype(valuesBegin) vPtr = valuesBegin + *i; V v = *vPtr;
		if (IsDefined(v))
		{
			Float64 c = 0;
			do	{
				Float64 w = weightsGetter->Get(*i);
				if (IsDefined(w))
					c += w;
			}	while (++i != e && valuesBegin[*i] == v);
			if (c > maxC)
			{
				maxC = c;
				resData = v;
			}
		}
		else
			while (++i != e && valuesBegin[*i] == v)
				;
	};
}

template<typename V>
void WeightedModusTotByIndexOrSet(
	const AbstrDataItem* valuesItem,
	const AbstrDataItem* weightItem,
	typename sequence_traits<V>::reference resData)
{
	if (valuesItem->GetAbstrDomainUnit()->IsCurrTiled())
		WeightedModusTotBySet  <V>(valuesItem, weightItem, resData);
	else
		WeightedModusTotByIndex<V>(valuesItem, weightItem, resData);
}

template<typename V>
void WeightedModusTotByTable(
	const AbstrDataItem* valuesItem,
	const AbstrDataItem* weightItem,
	typename sequence_traits<V>::reference resData, 
	const typename Unit<V>::range_t& valuesRange)
{
	UInt32 vCount = Cardinality(valuesRange);
	std::vector<Float64> buffer(vCount, 0);
	std::vector<Float64>::iterator
		bufferB = buffer.begin();

	for (tile_id t =0, tn = valuesItem->GetAbstrDomainUnit()->GetNrTiles(); t!=tn; ++t)
	{
		auto valuesLock  = const_array_cast<V>(valuesItem)->GetLockedDataRead(t);
		auto valuesIter  = valuesLock.begin(),
		     valuesEnd   = valuesLock.end();
		OwningPtr<AbstrValueGetter<Float64>> weightsGetter = WeightGetterCreator(weightItem, t).Create();

		SizeT weightIter  = 0;

		for (; valuesIter != valuesEnd; ++weightIter, ++valuesIter)
		{
			UInt32 v = Range_GetIndex_checked(valuesRange, *valuesIter);
			if (v < vCount)
				bufferB[v] += weightsGetter->Get(weightIter);
		}
	}

	vCount = argmax<Float64>(bufferB, vCount);
	if (IsDefined(vCount))
		resData = Range_GetValue_naked(valuesRange, vCount);
	else
		resData = UNDEFINED_OR_ZERO(V);
}

template<typename V>
void WeightedModusTotDispatcher(
	const AbstrDataItem* valuesItem
,	const AbstrDataItem* weightItem
,	typename sequence_traits<V>::reference resData
,	const inf_type_tag*
)
{
	// NonCountable values; go for Set implementation
	WeightedModusTotByIndexOrSet<V>(
		valuesItem
	,	weightItem
	,	resData
	);
}

// make tradeoff between 
//      ModusPartTable: O(n+v*p)           processing with O(v*p) temp memory
//	and ModusPartSet:   O(n*log(min(n,v))) processing with O(t) temp memory with t <= min(n,v*p)
// when v is not countable(such as string, float), always choose the second method
//
// Note that ModusTotal is a special case of ModusPatial with p=1
//
// When memory condition doesnt favour Set: n >= v*p
// then Table time O(n+v*p) <= O(2n) < O(n*log(min(n,v))
// Thus, tradeof is made at v*p <= n.

template<typename V>
void WeightedModusTotDispatcher(
	const AbstrDataItem* valuesItem
,	const AbstrDataItem* weightItem
,	typename sequence_traits<V>::reference  resData
,	const ord_type_tag*
)
{
	typename Unit<V>::range_t valuesRange = GetRange<V>( valuesItem );

	// Countable values; go for Table if sensible
	UInt32
		n = valuesItem->GetAbstrDomainUnit()->GetCount(),
		v = Cardinality(valuesRange);

	if	(	IsDefined(v)
		&&	v <= n
		&&	OnlyDefinedCheckRequired(valuesItem) // memory condition v*p<=n, thus TableTime <= 2n.
		)
		WeightedModusTotByTable<V>(
			valuesItem
		,	weightItem
		,	resData
		,	valuesRange
		);
	else
		WeightedModusTotByIndexOrSet<V>(
			valuesItem
		,	weightItem
		,	resData
		);
}

template<typename V>
void WeightedModusTotDispatcher(
	const AbstrDataItem* valuesItem
,	const AbstrDataItem* weightItem
,	typename sequence_traits<V>::reference  resData
,	const bool_type_tag*
)
{
	WeightedModusTotByTable<V>(
		valuesItem
	,	weightItem
	,	resData
	,	GetRange<V>(valuesItem)
	);
}

// *****************************************************************************
//											WeightedModusPart
// *****************************************************************************

// assume v >> n; time complexity: n*log(min(v, n))
template<typename V, typename OIV>
void WeightedModusPartBySet(
	const AbstrDataItem* valuesItem, const AbstrDataItem* weightItem, const AbstrDataItem* indicesItem,
	OIV resBegin, 
	SizeT pCount)  // countable dommain unit of result; P can be Void.
{
	typedef std::pair<SizeT, V> value_type;
	std::map<value_type, Float64> counters;

	for (tile_id t=0, tn= valuesItem->GetAbstrDomainUnit()->GetNrTiles(); t!=tn; ++t)
	{
		auto valuesLock  = const_array_cast<V>(valuesItem )->GetLockedDataRead(t);
		auto valuesIter  = valuesLock.begin(),
			 valuesEnd   = valuesLock.end();

		OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(indicesItem, t);
		OwningPtr<AbstrValueGetter<Float64>> weightsGetter = WeightGetterCreator::Create(weightItem, t);

		SizeT i=0;
		for (; valuesIter != valuesEnd; ++i, ++valuesIter)
			if (IsDefined(*valuesIter))
			{
				Float64 weight = weightsGetter->Get(i);
				if (IsDefined(weight))
				{
					SizeT p = indexGetter->Get(i);
					if (IsDefined(p))
					{
						dms_assert(p < pCount);
						counters[value_type(p, *valuesIter)] += weight;
					}
				}
			}
	}
	auto i = counters.begin(), e = counters.end();
	while (i != e)
	{
		SizeT p = i->first.first;
		dms_assert( IsDefined(p) );
		dms_assert( p < pCount );
		Float64 maxC = 0;
		do 
		{
			dms_assert( i->second>0);
			if ( i->second > maxC)
			{
				maxC        = i->second;
				resBegin[p] = i->first.second;
			}
		}	while (++i != e && i->first.first == p);
	}
}

// assume v >> n; time complexity: n*log(min(v, n))
template<typename V, typename OIV>
void WeightedModusPartByIndex(
	const AbstrDataItem* valuesItem,
	const AbstrDataItem* weightItem,
	const AbstrDataItem* indicesItem,
	OIV resBegin, SizeT pCount)  // countable dommain unit of result; P can be Void.
{
	auto valuesLock  = const_array_cast<V>(valuesItem )->GetLockedDataRead();
	auto valuesBegin = valuesLock.begin(),
	     valuesEnd   = valuesLock.end();

	OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(indicesItem, no_tile);
	OwningPtr<AbstrValueGetter<Float64>> weightsGetter = WeightGetterCreator::Create(weightItem);

	SizeT n = valuesEnd - valuesBegin;
	OwningPtrSizedArray<SizeT> index(n, dont_initialize MG_DEBUG_ALLOCATOR_SRC("WeightedModusPartByIndex: index"));
	auto i = index.begin(), e = index.end(); assert(e - i == n);
	make_indexP_in_existing_span(i, e, indexGetter, valuesBegin);

	while (i != e)
	{
		SizeT p = indexGetter->Get(*i);
		if (!IsDefined(p))
		{
			++i;
			continue;
		}
		else
		{
			dms_assert(p < pCount);
			Float64 maxC = MIN_VALUE(Float64);
			do
			{
				auto vPtr = valuesBegin + *i; V v = *vPtr;
				if (IsDefined(*vPtr))
				{
					Float64 c = 0;
					do	{
						Float64 w = weightsGetter->Get(*i);
						if (IsDefined(w))
							c += w; 
					}	while (++i != e && valuesBegin[*i]   == v && indexGetter->Get(*i) == p);
					if ( c > maxC )
					{
						maxC = c;
						resBegin[p] = v;
					}
				}
				else
				{
					while (++i != e && valuesBegin[*i] == v && indexGetter->Get(*i) == p)
						;
				}
			}	while (i != e && indexGetter->Get(*i) == p);
		}
	}
}

template<typename V, typename OIV>
void WeightedModusPartByIndexOrSet(const AbstrDataItem* valuesItem, const AbstrDataItem* weightItem, const AbstrDataItem* indicesItem, OIV resBegin, SizeT pCount)  // countable dommain unit of result; P can be Void.
{
	fast_fill(resBegin, resBegin+pCount, UNDEFINED_OR_ZERO(V));

	if (valuesItem->GetAbstrDomainUnit()->IsCurrTiled())
		WeightedModusPartBySet  <V, OIV>(valuesItem, weightItem, indicesItem, resBegin, pCount);
	else
		WeightedModusPartByIndex<V, OIV>(valuesItem, weightItem, indicesItem, resBegin, pCount);
}

template<typename V, typename OIV>
void WeightedModusPartByTable(
	const AbstrDataItem* valuesItem, 
	const AbstrDataItem* weightItem, 
	const AbstrDataItem* indicesItem, 
	OIV resBegin, 
	typename Unit<V>::range_t valuesRange, SizeT pCount)  // countable dommain unit of result; P can be Void.
{
	SizeT vCount = Cardinality(valuesRange);
	std::vector<Float64> buffer(vCount*pCount, 0);
	std::vector<Float64>::iterator bufferB = buffer.begin();

	for (tile_id t =0, tn = valuesItem->GetAbstrDomainUnit()->GetNrTiles(); t!=tn; ++t)
	{
		auto valuesLock  = const_array_cast<V>(valuesItem )->GetLockedDataRead(t);
		auto valuesIter  = valuesLock.begin(),
		     valuesEnd   = valuesLock.end();

		OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(indicesItem, t);
		OwningPtr<AbstrValueGetter<Float64>> weightsGetter = WeightGetterCreator::Create(weightItem, t);

		SizeT weightIter = 0;
		Float64 weight;
		SizeT i=0;
		for (; valuesIter != valuesEnd; ++weightIter, ++i, ++valuesIter)
			if (IsDefined(*valuesIter) && IsDefined(weight = weightsGetter->Get(weightIter)))
			{
				dms_assert(IsIncluding(valuesRange, *valuesIter)); // PRECONDITION
				UInt32 vi = Range_GetIndex_naked(valuesRange, *valuesIter);
				dms_assert(vi < vCount);
				SizeT pi = indexGetter->Get(i);
				if (IsNotUndef(pi))
				{
					dms_assert(pi < pCount);
					bufferB[pi * vCount + vi] += weight;
				}
			}
	}

	for(OIV resEnd = resBegin + pCount; resBegin != resEnd; ++resBegin)
	{
		SizeT i = argmax<Float64>(bufferB, vCount);
		if (IsDefined(i))
			*resBegin = Range_GetValue_naked(valuesRange, i);
		else
			*resBegin = UNDEFINED_OR_ZERO(V);
		bufferB += vCount;
	}
	dms_assert(bufferB == buffer.end());
}

template<typename V, typename OIV>
void WeightedModusPartDispatcher(const AbstrDataItem* valuesItem, const AbstrDataItem* weightItem, const AbstrDataItem* indicesItem, OIV resBegin, SizeT nrP
	, const inf_type_tag*)
{
	// NonCountable values; go for Set implementation
	WeightedModusPartByIndexOrSet<V, OIV>(valuesItem, weightItem, indicesItem, resBegin, nrP);
}

// make tradeoff between 
//      ModusPartTable: O(n+v*p)           processing with O(v*p) temp memory
//	and ModusPartSet:   O(n*log(min(n,v))) processing with O(t) temp memory with t <= min(n,v*p)
// when v is not countable(such as string, float), always choose the second method
//
// Note that ModusTotal is a special case of ModusPatial with p=1
//
// When memory condition doesnt favour Set: n >= v*p
// then Table time O(n+v*p) <= O(2n) < O(n*log(min(n,v))
// Thus, tradeof is made at v*p <= n.

template<typename V, typename OIV>
void WeightedModusPartDispatcher(
	const AbstrDataItem* valuesItem,
	const AbstrDataItem* weightItem,
	const AbstrDataItem* indicesItem,
	OIV resBegin,
	SizeT nrP
,	const int_type_tag*
)
{
	typename Unit<V>::range_t valuesRange = GetRange<V>(valuesItem);

	dms_assert(IsDefined(valuesRange)); //we already made result with p as domainUnit, thus count must be known and managable.
	dms_assert(!valuesRange.empty());   //we already made result with p as domainUnit, thus count must be known and managable.

	// Countable values; go for Table if sensible
	row_id
		n = valuesItem->GetAbstrDomainUnit()->GetCount(),
		v = valuesRange.empty() ? MAX_VALUE(row_id) : Cardinality(valuesRange);

	dms_assert(IsNotUndef(nrP)); //consequence of the checks on indexRange: values Unit of index has been used as domain of the result

	if	(	IsDefined(v)
		&& (!nrP || v <= n / nrP)
		&&	OnlyDefinedCheckRequired(valuesItem)
		) // memory condition v*p<=n, thus TableTime <= 2n.
		WeightedModusPartByTable<V>(
			valuesItem, weightItem,	indicesItem
		,	resBegin
		,	valuesRange
		,	nrP
		);
	else
		WeightedModusPartByIndexOrSet<V>(
			valuesItem, weightItem,	indicesItem
		,	resBegin
		,	nrP
		);
}

template<typename V, typename OIV>
void WeightedModusPartDispatcher(
	const AbstrDataItem* valuesItem,
	const AbstrDataItem* weightItem,
	const AbstrDataItem* indicesItem,
	OIV  resBegin, 
	SizeT nrP,
	const bool_type_tag*
)
{
	WeightedModusPartByTable<V>(
		valuesItem, weightItem,	indicesItem
	,	resBegin
	,	GetRange<V>(valuesItem)
	,	nrP
	);
}

template <typename V> 
struct ModusTotal : AbstrOperAccTotUni
{
	typedef V                     ValueType;
	typedef DataArray<ValueType>  Arg1Type;   // value vector
	typedef DataArray<ValueType>  ResultType; // will contain the first most occuring value
			
public:
	ModusTotal(AbstrOperGroup* gr) 
		:	AbstrOperAccTotUni(gr
			,	ResultType::GetStaticClass(), Arg1Type::GetStaticClass()
			,	arg1_values_unit, COMPOSITION(V)
			)
	{}

	void Calculate(DataWriteLock& res, const AbstrDataItem* arg1A, ArgRefs args, std::vector<ItemReadLock> readLocks) const override
	{
		ResultType* result = mutable_array_cast<ValueType>(res);
		dms_assert(result);
		auto  resData = result->GetDataWrite();

		ModusTotDispatcher<V>(
			arg1A,
			resData[0],
			TYPEID(elem_traits<V>)
		);
	}
};

template <typename V> 
struct WeightedModusTotal : AbstrOperAccTotBin
{
	typedef V             ValueType;
	typedef DataArray<V>  Arg1Type;   // value vector
	typedef AbstrDataItem Arg2Type;   // weight vector
	typedef DataArray<V>  ResultType; // will contain the first most occuring value
			
public:
	WeightedModusTotal(AbstrOperGroup* gr) 
		:	AbstrOperAccTotBin(gr
			,	ResultType::GetStaticClass(), Arg1Type::GetStaticClass(), Arg2Type::GetStaticClass()
			,	arg1_values_unit, COMPOSITION(V)
			)
	{}

	void Calculate(DataWriteLock& res, const AbstrDataItem* arg1A, const AbstrDataItem* arg2A) const override
	{
		ResultType* result = mutable_array_cast<V>(res);
		dms_assert(result);
		auto resData = result->GetDataWrite();

		WeightedModusTotDispatcher<V>(
			arg1A, arg2A,
			resData[0], 
			TYPEID(elem_traits<V>)
		);
	}
};

template <typename V> 
struct ModusPart : OperAccPartUniWithCFTA<V, V>
{
	typedef V                     ValueType;
	typedef DataArray<ValueType>  Arg1Type;   // value vector
	typedef AbstrDataItem         Arg2Type;   // index vector

	typedef DataArray<ValueType>  ResultType; // will contain the first most occuring value per index value
	using base_type = OperAccPartUniWithCFTA<V, V>;
	using ProcessDataInfo = base_type::ProcessDataInfo;

	ModusPart(AbstrOperGroup* gr) 
		: base_type(gr, arg1_values_unit)
	{}

	void ProcessData(ResultType* result, ProcessDataInfo& pdi) const override
	{
		assert(result);
		auto resData = result->GetDataWrite(no_tile, dms_rw_mode::write_only_all);
		dbg_assert(resData.size()  == pdi.arg2A->GetAbstrValuesUnit()->GetCount());
		auto resBegin = resData.begin();

		if constexpr(is_bitvalue_v<V>)
			ModusPartByTable<V>(pdi.arg2A, std::move(pdi.values_fta), std::move(pdi.part_fta), resBegin, typename Unit<V>::range_t(0, 1 << nrbits_of_v<V>), pdi.resCount);
		else
		{
			// make tradeoff between 
			//      ModusPartTable: O(n+v*p)           processing with O(v*p) temp memory
			//	and ModusPartSet:   O(n*log(min(n,v))) processing with O(t) temp memory with t <= min(n,v*p)
			// when v is not countable(such as string, float), always choose the second method
			//
			// Note that ModusTotal is a special case of ModusPatial with p=1
			//
			// When memory condition doesnt favour Set: n >= v*p
			// then Table time O(n+v*p) <= O(2n) < O(n*log(min(n,v))
			// Thus, tradeof is made at v*p <= n.

			// Countable values; go for Table if sensible
			SizeT v = MAX_VALUE(SizeT);
			if constexpr (is_integral_v<field_of_t<V>>)
			{
				auto range = pdi.valuesRangeData->GetRange();
				if (!range.empty())
					v =  Cardinality(range);
			}

			assert(IsNotUndef(pdi.resCount)); //consequence of the checks on indexRange

			if (IsDefined(v)
				//		&& (!resCount || v / map_node_type_size<V> <= n / resCount / sizeof(SizeT))
				&& (!pdi.resCount || v <= pdi.n / pdi.resCount)
				) // memory condition v*p<=n, thus TableTime <= 2n.
				ModusPartByTable<V>(pdi.arg2A, std::move(pdi.values_fta), std::move(pdi.part_fta), resBegin, pdi.valuesRangeData->GetRange(), pdi.resCount);
			else
				ModusPartByIndexOrSet<V>(pdi.arg2A, std::move(pdi.values_fta), std::move(pdi.part_fta), resBegin, pdi.resCount);
		}
	}
};

template <typename V> 
struct WeightedModusPart : public AbstrOperAccPartBin
{
	typedef V                     ValueType;
	typedef DataArray<ValueType>  Arg1Type;   // value vector
	typedef AbstrDataItem         Arg2Type;   // weight vector
	typedef AbstrDataItem         Arg3Type;   // index vector
	typedef DataArray<ValueType>  ResultType; // will contain the first most occuring value per index value
			
	WeightedModusPart(AbstrOperGroup* gr) 
		:	AbstrOperAccPartBin(gr
			,	ResultType::GetStaticClass()
			,	Arg1Type::GetStaticClass(), Arg2Type::GetStaticClass(), Arg3Type::GetStaticClass()
			,	arg1_values_unit, COMPOSITION(ValueType)
			)
	{}

	// Override Operator
	void Calculate(DataWriteLock& res,
		const AbstrDataItem* arg1A, 
		const AbstrDataItem* arg2A,
		const AbstrDataItem* arg3A
	) const override
	{
		ResultType* result = mutable_array_cast<ValueType>(res);
		dms_assert(result);
		auto resData = result->GetLockedDataWrite();

		dbg_assert(resData.size() == res->GetTiledRangeData()->GetRangeSize()); // DataWriteLock was set by caller and p3 is domain of res

		WeightedModusPartDispatcher<V>(
			arg1A, arg2A, arg3A,
			resData.begin(), 
			arg3A->GetAbstrValuesUnit()->GetCount(),
			TYPEID(elem_traits<V>)
		);
	}
};


// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

namespace 
{
	CommonOperGroup cogModus("modus");
	CommonOperGroup cogModusW("modus_weighted");

	template <typename V>
	struct ModusInst
	{
		ModusInst()
			: mt(&cogModus)
			, wmt(&cogModusW)
			, mp(&cogModus)
			, wmp(&cogModusW)
		{}

	private:
		ModusTotal<V>         mt;
		WeightedModusTotal<V> wmt;
		ModusPart<V>          mp;
		WeightedModusPart<V> wmp;
	};

	tl_oper::inst_tuple<typelists::aints, ModusInst<_> > modusOpers;

//	ModusInst<SharedStr> mpString; //TODO, ook TODO: optimize dispatchers voor (U)Int4/2
}
