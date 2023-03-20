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

#include "ser/AsString.h"
#include "set/VectorFunc.h"
#include "geo/StringBounds.h"
#include "geo/GeoSequence.h"

#include "CheckedDomain.h"
#include "DataArray.h"
#include "DataItemClass.h"
#include "Metric.h"
#include "ParallelTiles.h"
#include "TileFunctorImpl.h"
#include "Unit.h"
#include "UnitClass.h"
#include "UnitProcessor.h"

#include "OperRelUni.h"
#include "lookup.h"

// *****************************************************************************
//                         lookup(E->T, T->V): E->V
//                         LookupOperators
// *****************************************************************************

class AbstrLookupOperator : public BinaryOperator
{
	ValueComposition m_VC;
public:
	AbstrLookupOperator(AbstrOperGroup& aog, const DataItemClass* arg1Cls, const DataItemClass* arg2Cls) : 
		BinaryOperator(&aog, arg2Cls, // result elem type == arg2 elem type
			arg1Cls, arg2Cls
		) 
		,	m_VC(arg2Cls->GetValuesType()->GetValueComposition())
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 2);

		const AbstrDataItem* arg1A= AsDataItem(args[0]);
		const AbstrDataItem* arg2A= AsDataItem(args[1]);

		dms_assert(arg1A);
		dms_assert(arg2A);

		const AbstrUnit* arg1ValuesA = arg1A->GetAbstrValuesUnit();
		const AbstrUnit* arg2DomainA = arg2A->GetAbstrDomainUnit();
		dms_assert(arg1ValuesA && arg2DomainA);


		const AbstrUnit* domainA = arg1A->GetAbstrDomainUnit();
		const AbstrUnit* valuesA = arg2A->GetAbstrValuesUnit();
		auto vc = m_VC;
		Unify(vc, arg2A->GetValueComposition());

		if (!resultHolder)
		{
			arg1ValuesA->UnifyDomain(arg2DomainA, "v1", "e2", UnifyMode(UM_Throw|UM_AllowDefaultLeft));

			dms_assert(domainA);
			dms_assert(valuesA);

			resultHolder = CreateCacheDataItem(domainA, valuesA, vc );
			if (valuesA->GetTSF(DSF_Categorical))
				resultHolder->SetTSF(DSF_Categorical);
		}

		if (mustCalc)
		{
			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			dms_assert(res);
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);

			DataCheckMode dcmArg1 = arg1A->GetCheckMode();
			if (dcmArg1 == DCM_CheckBoth || !(dcmArg1 & DCM_CheckRange) && MustCheckRange(arg1A, arg2DomainA))
				dcmArg1 = DCM_CheckRange; // if <null> is within the data range, lookup the null value in the data value array.

			auto tn = domainA->GetNrTiles();
			auto wrappedValuesArray = MakeValuesArray(arg2A);
			if (IsMultiThreaded3() && (tn > 1) && (LTF_ElementWeight(arg1A) <= LTF_ElementWeight(res)) && tn > arg2DomainA->GetNrTiles())
				AsDataItem(resultHolder.GetOld())->m_DataObject = CreateFutureTileFunctor(arg1A, dcmArg1, valuesA, arg2DomainA, wrappedValuesArray MG_DEBUG_ALLOCATOR_SRC("res->md_FullName + : lookup"));
			else
			{
				DataWriteLock resLock(res);

				parallel_tileloop(tn, [this, &resLock, arg1A, dcmArg1, arg2DomainA, &wrappedValuesArray](tile_id t)->void
					{
						Calculate(resLock, arg1A, dcmArg1, arg2DomainA, wrappedValuesArray, t);
					});

				resLock.Commit();
			}
		}
		return true;
	}
	virtual bool MustCheckRange(const AbstrDataItem* arg1A, const AbstrUnit* arg2Domain) const =0;
	virtual auto CreateFutureTileFunctor(const AbstrDataItem* arg1A, DataCheckMode dcmArg1, const AbstrUnit* valuesA, const AbstrUnit* arg2DomainA, const std::any& wrappedValuesArray MG_DEBUG_ALLOCATOR_SRC_ARG) const -> SharedPtr<const AbstrDataObject> = 0;
	virtual void Calculate(AbstrDataObject* res, const AbstrDataItem* arg1A, DataCheckMode dcmArg1, const AbstrUnit* arg2DomainA, const std::any& wrappedValuesArray, tile_id t) const =0;
	virtual std::any MakeValuesArray(const AbstrDataItem* arg2A) const = 0;
};

// *****************************************************************************
//                         lookup(E->T, T->V): E->V
//                         LookupOperators
// *****************************************************************************

template <class T, class V>
class LookupOperator : public AbstrLookupOperator
{
	typedef DataArray<T>              Arg1Type;  // thematic attribute
	typedef typename Unit<T>::range_t Arg1RangeType;
	typedef DataArray<V>              Arg2Type;  // classification sheet
	typedef DataArray<V>              ResultType;
         
public:
	LookupOperator(AbstrOperGroup& aog) : AbstrLookupOperator(aog, Arg1Type::GetStaticClass(), Arg2Type::GetStaticClass()) 
	{}

	std::any MakeValuesArray(const AbstrDataItem* arg2A) const override
	{
		const Arg2Type* arg2 = const_array_cast<V>(arg2A);
		dms_assert(arg2);
		return arg2->GetDataRead(); // ShadowTile
	}

	bool MustCheckRange(const AbstrDataItem* arg1A, const AbstrUnit* arg2Domain) const override
	{
		const Arg1Type* arg1 = const_array_cast<T>(arg1A);
		dms_assert(arg1);

		auto arg2_DomainUnit = debug_cast<const Unit<T>*>(arg2Domain);
		dms_assert(arg2_DomainUnit);

		Arg1RangeType actualIndexRange = arg2_DomainUnit->GetRange();
		Arg1RangeType formalIndexRange = arg1->GetValueRangeData()->GetRange();
		return !IsIncluding(actualIndexRange, formalIndexRange);
	}


	auto CreateFutureTileFunctor(const AbstrDataItem* arg1A, DataCheckMode dcmArg1, const AbstrUnit* valuesA, const AbstrUnit* arg2DomainA, const std::any& wrappedValuesArray MG_DEBUG_ALLOCATOR_SRC_ARG) const -> SharedPtr<const AbstrDataObject> override
	{
		assert(valuesA);
		assert(arg2DomainA);

		auto tileRangeData = AsUnit(arg1A->GetAbstrDomainUnit()->GetCurrRangeItem())->GetTiledRangeData();
		auto valuesUnit = debug_cast<const Unit<field_of_t<V>>*>(valuesA->GetCurrRangeItem());

		auto arg1 = const_array_cast<T>(arg1A); dms_assert(arg1);

		auto arg2_DomainUnit = debug_cast<const Unit<T>*>(arg2DomainA->GetCurrRangeItem());
		dms_assert(arg2_DomainUnit);
		dms_assert(arg2_DomainUnit->GetInterestCount());
		Arg1RangeType actualIndexRange = arg2_DomainUnit->GetRange();

		auto valuesData = std::any_cast<typename DataArrayBase<V>::locked_cseq_t>(wrappedValuesArray);
		dms_assert(valuesData);

		using prepare_data = SharedPtr<typename Arg1Type::future_tile>;
		auto futureTileFunctor = make_unique_FutureTileFunctor<V, prepare_data, false>(tileRangeData, get_range_ptr_of_valuesunit(valuesUnit), tileRangeData->GetNrTiles()
			, [arg1](tile_id t) { return arg1->GetFutureTile(t); }
			, [this, dcmArg1, actualIndexRange, valuesData  MG_DEBUG_ALLOCATOR_SRC_PARAM](sequence_traits<V>::seq_t resData, prepare_data futureData)
			{
				this->CalcTile(resData, futureData->GetTile().get_view(), dcmArg1, actualIndexRange, valuesData  MG_DEBUG_ALLOCATOR_SRC_PARAM);
			}
			MG_DEBUG_ALLOCATOR_SRC_PARAM
		);

		return futureTileFunctor.release();
	}

	void Calculate(AbstrDataObject* res, const AbstrDataItem* arg1A, DataCheckMode dcmArg1, const AbstrUnit* arg2DomainA, const std::any& wrappedValuesArray, tile_id t) const override
	{
		const Arg1Type* arg1 = const_array_cast<T>(arg1A);
		dms_assert(arg1);
		auto indexData = arg1->GetTile(t);

		auto resultData = mutable_array_cast<V>(res)->GetWritableTile(t);

		auto arg2_DomainUnit = debug_cast<const Unit<T>*>(arg2DomainA);
		dms_assert(arg2_DomainUnit);
		dms_assert(arg2_DomainUnit->GetInterestCount());
		Arg1RangeType actualIndexRange = arg2_DomainUnit->GetRange();

		auto valuesData = std::any_cast<typename DataArrayBase<V>::locked_cseq_t>(wrappedValuesArray);
		dms_assert(valuesData);

		CalcTile(resultData, indexData, dcmArg1, actualIndexRange, valuesData  MG_DEBUG_ALLOCATOR_SRC("res->md_SrcStr"));
	}

	void CalcTile(sequence_traits<V>::seq_t resultData, sequence_traits<T>::cseq_t indexData, DataCheckMode dcmArg1, Arg1RangeType actualIndexRange, sequence_traits<V>::cseq_t valuesData MG_DEBUG_ALLOCATOR_SRC_ARG) const
	{
		dms_assert(resultData.size() == indexData.size());
		if (!indexData.size())
			return;

		dms_assert(valuesData.size() == Cardinality(actualIndexRange));// <= domain of valid indexData

		lookup_best(resultData.begin(), resultData.end(), indexData.begin(), valuesData.begin(), actualIndexRange, dcmArg1);
	}
};


// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"
#include "LispTreeType.h"

CommonOperGroup cog_lookup(token::lookup);

namespace 
{
	CommonOperGroup cog_collect_by_org_rel(token::collect_by_org_rel);


	template <typename V> struct lookup_instances
	{
		using typefunc =
			tl_oper::tuple_func<
				tl::transform<
					typelists::domain_elements
				,	LookupOperator<_, V>
				>
			,	AbstrOperGroup&
			>;
		using type = typename typefunc::type;
	};

	tl_oper::inst_tuple<typelists::value_elements, lookup_instances<_>, AbstrOperGroup&> operLookup(cog_lookup);
	tl_oper::inst_tuple<typelists::value_elements, lookup_instances<_>, AbstrOperGroup&> operCollectByOrgRel(cog_collect_by_org_rel);

} // end anonymous namespace

/******************************************************************************/

