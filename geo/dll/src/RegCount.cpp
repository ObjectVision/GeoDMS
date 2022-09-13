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

#include "GeoPCH.h"
#pragma hdrstop

#include "RtcTypeLists.h"
#include "act/any.h"
#include "utl/TypeListOper.h"

#include "OperationContext.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"
#include "UnitCreators.h"

// *****************************************************************************
//                            RegCountOperator
// een operator om de 27 pcounts op de ontwerpkaart in 1 keer te doen.
//	reg_count<K>: (
//		tekenkaart: GRID->K; 
//		naam: K->STRING;
//		regiogridcontainer: { regio[i]: GRID->R[I] }; 
//		indelingen: K->regio[I]
//	): { naam[K]: R[ I[K] ] -> #GRID }
// *****************************************************************************

#include "DataLockContainers.h"
#include "UnitProcessor.h"
#include "IndexGetterCreator.h"

namespace 
{

oper_arg_policy oap_regCount[4] = { 
	oper_arg_policy::calc_as_result,
	oper_arg_policy::calc_always,
	oper_arg_policy::subst_with_subitems,
	oper_arg_policy::calc_always
};

SpecialOperGroup sog_regCount  ("reg_count"       , 4, oap_regCount);
SpecialOperGroup sog_regCount32("reg_count_uint32", 4, oap_regCount);
SpecialOperGroup sog_regCount16("reg_count_uint16", 4, oap_regCount);
SpecialOperGroup sog_regCount8 ("reg_count_uint8" , 4, oap_regCount);

typedef UInt32 ActorTypeIndex;
typedef SizeT  PartitionIndex;

struct RegionInfo
{
	RegionInfo(const AbstrDataItem* partition, AbstrDataItem* result)
		:	m_Partition(partition)
		,	m_NrParts (UNDEFINED_VALUE(SizeT))
		,	m_Result  (result)
	{}

	RegionInfo(RegionInfo&& rhs)  // move ctor
		: m_Partition(std::move(rhs.m_Partition))
		, m_ReadLock(std::move(rhs.m_ReadLock))
		, m_IndexGetter(std::move(rhs.m_IndexGetter))
		, m_NrParts(std::move(rhs.m_NrParts))
		, m_Result(std::move(rhs.m_Result))
		, m_WriteLock(std::move(rhs.m_WriteLock))
	{
		throwIllegalAbstract(MG_POS, "RegionInfo::move ctor"); // reserve and emplace_back should prevent ever moving or copying this
	}

	SharedPtr<const AbstrDataItem>  m_Partition;
	DataReadLock                    m_ReadLock;
	OwningPtr<IndexGetter>          m_IndexGetter;
	SizeT                           m_NrParts;
	SharedPtr<AbstrDataItem>        m_Result;

	DataWriteLock                   m_WriteLock;
private:
	RegionInfo(const RegionInfo&);
};

struct RegionInfoArray : std::vector<RegionInfo>
{
	ConstUnitRef m_ResUnit;
	RegionInfoArray() = default;
	RegionInfoArray(RegionInfoArray&&) = default;

private:
	RegionInfoArray(const RegionInfoArray&) = delete;
	void operator = (const RegionInfoArray&) = delete;
};

void PrepareTile(RegionInfoArray* regionInfoArrayPtr, tile_id t)
{
	for (auto i=regionInfoArrayPtr->begin(), e=regionInfoArrayPtr->end(); i!=e; ++i)
		if (i->m_Partition)
			i->m_IndexGetter.assign( IndexGetterCreator::Create(i->m_Partition, t) );
}

template <typename ActorType>
struct RegTileCounterBase : UnitProcessor
{
	template <typename CounterType>
	void VisitImpl(const Unit<CounterType>* inviter) const
	{
		dms_assert(regionInfoArrayPtr);
		ActorTypeIndex n = regionInfoArrayPtr->size();
		std::vector<DataArray<CounterType>::locked_seq_t> countsArray;
		countsArray.reserve(n);
		for (auto i=regionInfoArrayPtr->begin(), e=regionInfoArrayPtr->end(); i!=e; ++i)
			countsArray.push_back(mutable_array_cast<CounterType>(i->m_WriteLock)->GetDataWrite());

		DataReadLock arg1Lock(arg1A);

		const AbstrUnit* gridDomain = arg1A->GetAbstrDomainUnit();
		typename Unit<ActorType>::range_t actorTypeRange = const_array_checkedcast<ActorType>(arg1A)->GetValueRangeData()->GetRange();

		tile_id te = gridDomain->GetNrTiles();
		for (tile_id t=0; t!=te; ++t)
		{
			auto arg1Data = const_array_cast<ActorType>(arg1A)->GetDataRead(t);
			PrepareTile(regionInfoArrayPtr, t);

			SizeT c = 0;
			for	(auto iLU = arg1Data.begin(), eLU = arg1Data.end(); iLU != eLU; ++c, ++iLU)
			{
				ActorTypeIndex lu = Range_GetIndex_checked(actorTypeRange, *iLU);
				if (lu >= n)
					continue;
				RegionInfo& ri = regionInfoArrayPtr->begin()[lu];
				PartitionIndex j = ri.m_Partition ? ri.m_IndexGetter->Get( c ) : 0;

				auto& counts = countsArray[lu];
				if (j < counts.size())
					++(counts[j]);
			}
		}
	}
	WeakPtr<RegionInfoArray> regionInfoArrayPtr;
	WeakPtr<const AbstrDataItem>     arg1A;
};

template <typename ActorType>
struct RegTileCounter :  boost::mpl::fold<typelists::uints, RegTileCounterBase<ActorType>, VisitorImpl<Unit<boost::mpl::_2>, boost::mpl::_1> >::type
{};


template <typename ActorType>
struct RegCountOperator : public QuaternaryOperator
{
	const UnitClass* m_CountUnitClass;
	RegCountOperator(AbstrOperGroup& regCountGroup, const UnitClass* countUnitClass)
		:	QuaternaryOperator(&regCountGroup
			,	TreeItem            ::GetStaticClass() // result container with per actortype name: counts per partition
			,	DataArray<ActorType>::GetStaticClass() // actortype indices per domain-element
			,	DataArray<SharedStr>::GetStaticClass() // name per actortype
			,	TreeItem            ::GetStaticClass() // container with domain partitionings per partitioning name
			,	DataArray<SharedStr>::GetStaticClass() // partitioning name per actortype
			)
		,	m_CountUnitClass(countUnitClass)
	{}

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, OperationContext* fc, LispPtr) const override
	{
		dms_assert(args.size() == 4);

		const AbstrDataItem* arg1A = AsDataItem(args[0]); dms_assert(arg1A);
		const AbstrDataItem* arg2A = AsDataItem(args[1]); dms_assert(arg2A);
		const TreeItem*      partitionContainer = GetItem(args[2]); dms_assert(partitionContainer);
		const AbstrDataItem* arg4A = AsDataItem(args[3]); dms_assert(arg4A);

		const AbstrUnit* gridDomain = arg1A->GetAbstrDomainUnit();
		dms_assert(gridDomain);

		auto actorTypeUnit = arg1A->GetAbstrValuesUnit(); // LANDUSE CLASSES
		dms_assert(actorTypeUnit);
		ActorTypeIndex n = actorTypeUnit->GetCount();

		actorTypeUnit->UnifyDomain(arg2A->GetAbstrDomainUnit(), UM_Throw);
		actorTypeUnit->UnifyDomain(arg4A->GetAbstrDomainUnit(), UM_Throw);

		RegionInfoArray regionInfoArray; regionInfoArray.reserve(n);
		regionInfoArray.m_ResUnit = (m_CountUnitClass)
			? (gridDomain->GetStaticClass() == m_CountUnitClass)
			? gridDomain
			: m_CountUnitClass->CreateDefault()
			: count_unit_creator(GetItems(args)).get_ptr();
		dms_assert(regionInfoArray.m_ResUnit);

		if (!resultHolder)
			resultHolder = TreeItem::CreateCacheRoot();

		DataReadLock arg2Lock(arg2A);
		DataReadLock arg4Lock(arg4A);
		for (ActorTypeIndex i = 0; i != n; ++i)
		{
			SharedStr partitionName = arg4A->GetValue<SharedStr>(i);
			SharedPtr<const AbstrDataItem> partition;
			DataControllerRef partitionResultHolder;
			if (!partitionName.empty())
			{
				partition = AsDynamicDataItem(partitionContainer->FindItem(partitionName));
				if (!partition)
					GetGroup()->throwOperErrorF("Partition %s not found in partition container %s",
						partitionName.c_str(),
						partitionContainer->GetFullName().c_str()
					);
				partition->UpdateMetaInfo();
				gridDomain->UnifyDomain(partition->GetAbstrDomainUnit(), UM_Throw);
			}
			auto className = arg2A->GetValue<SharedStr>(i);
			if (className.empty())
				throwErrorF("RegCount", "no ItemName for row %d of 2nd argument", i);
			TokenID nameID = GetTokenID_mt(className.c_str());
			dms_assert(nameID);
			const AbstrUnit* regionalDomain = partition ? partition->GetAbstrValuesUnit() : Unit<Void>::GetStaticClass()->CreateDefault();
			AbstrDataItem* resultItem = CreateDataItem(resultHolder, nameID, regionalDomain, regionInfoArray.m_ResUnit);
			dms_assert(resultItem);
			regionInfoArray.emplace_back(partition, resultItem);
			if (partition)
			{
				fc->AddDependency(partition->GetCheckedDC()); // requires Meta info.
//				fc->AddDependency(partition->GetAbstrValuesUnit()); // and of valuesunit, or is that included?
			}
		}
		fc->m_MetaInfo = make_noncopyable_any<RegionInfoArray>(std::move(regionInfoArray));
	}

	bool CalcResult(TreeItemDualRef& resultHolder, const ArgRefs& args, OperationContext* fc, Explain::Context* context) const override
	{
		dms_assert(resultHolder);
		RegionInfoArray& regionInfoArray = *noncopyable_any_cast<RegionInfoArray>(&fc->m_MetaInfo);

		const AbstrDataItem* arg1A = AsDataItem(args[0]); dms_assert(arg1A);
		const AbstrDataItem* arg2A = AsDataItem(args[1]); dms_assert(arg2A);
		const TreeItem*      partitionContainer = GetItem(args[2]); dms_assert(partitionContainer);
		const AbstrDataItem* arg4A = AsDataItem(args[3]); dms_assert(arg4A);

		auto actorTypeRange = const_array_cast<ActorType>(arg1A)->GetValueRangeData(); // LANDUSE CLASSES
		dms_assert(actorTypeRange);
		ActorTypeIndex n = Cardinality(actorTypeRange->GetRange());

		// ================ lock data

		for (ActorTypeIndex i=0; i!=n; ++i)
		{
			RegionInfo& ri = regionInfoArray[i];
			ri.m_NrParts  = ri.m_Partition ? ri.m_Partition->GetAbstrValuesUnit()->GetCount() : 1;
			ri.m_ReadLock  = DataReadLock(ri.m_Partition);
			ri.m_WriteLock = DataWriteLock(ri.m_Result, dms_rw_mode::write_only_mustzero);
		}

		// ================ do calc

		RegTileCounter<ActorType> regTileCounter;
		regTileCounter.regionInfoArrayPtr = &regionInfoArray;
		regTileCounter.arg1A = arg1A;

		regionInfoArray.m_ResUnit->InviteUnitProcessor(regTileCounter);

		// ================ store results

		for (ActorTypeIndex i=0; i!=n; ++i)
			regionInfoArray[i].m_WriteLock.Commit();

		resultHolder->SetIsInstantiated();
		return true;
	}
};

template <typename ActorType>
struct RegCountOperators
{
	RegCountOperator<ActorType> m_AsDomain, m_UInt32, m_UInt16, m_UInt8;

	RegCountOperators()
	:	m_AsDomain(sog_regCount, nullptr)
	,	m_UInt32(sog_regCount32, Unit<UInt32>::GetStaticClass())
	,	m_UInt16(sog_regCount16, Unit<UInt16>::GetStaticClass())
	,	m_UInt8 (sog_regCount8 , Unit<UInt8 >::GetStaticClass())
	{}
};

}	// anonymous namespace

// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************


namespace 
{
	tl_oper::inst_tuple<typelists::domain_elements, RegCountOperators<_> > regCountOpers;
}	// end anonymous namespace

/******************************************************************************/

