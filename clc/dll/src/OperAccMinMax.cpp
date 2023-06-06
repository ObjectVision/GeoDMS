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

#include "OperAccUniNum.h"
#include "ptr/Resource.h"
#include "IndexAssigner.h"
#include "ParallelTiles.h"

CommonOperGroup cog_Min("min");
CommonOperGroup cog_Max("max");
CommonOperGroup cog_MinIndex("min_index", oper_policy::dynamic_result_class);
CommonOperGroup cog_MaxIndex("max_index", oper_policy::dynamic_result_class);

// rlookup(E1->V, E->V): E1->E


class AbstrMinMaxIndexOperator : public BinaryOperator
{
	typedef AbstrDataItem      ResultType;

public:
	AbstrMinMaxIndexOperator(AbstrOperGroup& gr, const Class* arg1Class)
		: BinaryOperator(&gr, ResultType::GetStaticClass(), arg1Class, AbstrDataItem::GetStaticClass())
	{
		m_NrOptionalArgs = 1;
	}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 1 || args.size() == 2);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrDataItem* arg2A = (args.size() == 1) ? nullptr : AsDataItem(args[1]);
		dms_assert(arg1A);
		
		const AbstrUnit* arg1_DomainUnit = arg1A->GetAbstrDomainUnit();
		dms_assert(arg1_DomainUnit);
		if (arg2A) arg1_DomainUnit->UnifyDomain(arg2A->GetAbstrDomainUnit(), "e1", "e2", UM_Throw);

		const AbstrUnit* arg2_ValuesUnit = (arg2A) ? arg2A->GetAbstrValuesUnit() : Unit<Void>::GetStaticClass()->CreateDefault();

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(arg2_ValuesUnit, arg1_DomainUnit);

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);

			tile_id nrTiles = arg1_DomainUnit->GetNrTiles();

			bool arg1HasUndefined = arg1A->HasUndefinedValues();

			SizeT p = arg2_ValuesUnit->GetCount();
			IndexAssignerSizeT indices(res, resLock.get(), no_tile, 0, p);
			fast_undefine(indices.m_Indices, indices.m_Indices + p);

			ResourceHandle valueArray;

			Calculate(p, valueArray, indices.m_Indices, arg1A, arg1HasUndefined, arg2A);

			indices.Store();
			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(SizeT p, ResourceHandle& valueArray, SizeT* indices, const AbstrDataItem* arg1A, bool arg1HasUndefined, const AbstrDataItem* arg2A) const = 0;
};

template <typename V, typename Comparator>
class MinMaxIndexOperator : public AbstrMinMaxIndexOperator
{
	typedef DataArray<V> ArgumentType;

public:
	MinMaxIndexOperator(AbstrOperGroup& og): AbstrMinMaxIndexOperator(og, DataArray<V>::GetStaticClass())
	{}

	void Calculate(SizeT nr_p, ResourceHandle& valueArray, SizeT* indices, const AbstrDataItem* arg1A, bool arg1HasUndefined, const AbstrDataItem* arg2A) const override
	{
		typedef sequence_traits<V>::container_type valueContainer;
		if (!valueArray)
			valueArray = makeResource<valueContainer>(nr_p, Comparator::template StartValue<V>());
		valueContainer& values = GetAs<valueContainer>(valueArray);

		auto valueData = const_array_cast<V>(arg1A)->GetLockedDataRead();

		OwningPtr<IndexGetter> indexGetter;
		if (arg2A)
			indexGetter = IndexGetterCreator::Create(arg2A, no_tile);
		SizeT p = 0;
		Comparator comp;
		for (SizeT i = 0, e = valueData.size(); i != e; ++i)
		{
			if (indexGetter)
				p = indexGetter->Get(i);
			if (p >= nr_p)
				continue;

			if (IsDefined(valueData[i]) && comp(valueData[i], values[p]))
			{
				values[p] = valueData[i];
				indices[p] = i;
			}
		}
	}
};

namespace
{
	OperAccUniNum::AggrOperators<min_total_best, min_partial_best, typelists::ranged_unit_objects> s_MinOpers(&cog_Min);
	OperAccUniNum::AggrOperators<max_total_best, max_partial_best, typelists::ranged_unit_objects> s_MaxOpers(&cog_Max);

	struct ltFunc { template<typename V, typename W> bool operator() (const V& a, const W& b) { return a < b; }; template<typename V> static V StartValue() { return MAX_VALUE(V);  } };
	struct gtFunc { template<typename V, typename W> bool operator() (const V& a, const W& b) { return b < a; }; template<typename V> static V StartValue() { return MIN_VALUE(V); }};

	tl_oper::inst_tuple<typelists::numerics, MinMaxIndexOperator<_, ltFunc>, AbstrOperGroup&> s_MinIndexOpers(cog_MinIndex);
	tl_oper::inst_tuple<typelists::numerics, MinMaxIndexOperator<_, gtFunc>, AbstrOperGroup&> s_MaxIndexOpers(cog_MaxIndex);
}
