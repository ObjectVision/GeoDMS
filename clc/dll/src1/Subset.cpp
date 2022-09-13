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

#include "mci/CompositeCast.h"
#include "mci/ValueClass.h"

#include "DataArray.h"
#include "DataItemClass.h"
#include "ParallelTiles.h"
#include "TileChannel.h"
#include "Unit.h"
#include "UnitClass.h"
#include "UnitProcessor.h"

// *****************************************************************************
//                         count_selected
// *****************************************************************************

template<typename Block>
SizeT count_bitvalues(bit_iterator<1, Block> sb, bit_iterator<1, Block> se)
{
	SizeT count = 0;
	for (; sb.m_NrElems && sb != se; ++sb)
		if (bit_value<1>(*sb))
			++count;
	while (sb.m_BlockData != se.m_BlockData)
	{
		if ((*sb.m_BlockData & bit_info<1, Block>::used_bits_mask) == 0)
			++sb.m_BlockData;
		else if ((*sb.m_BlockData & bit_info<1, Block>::used_bits_mask) == bit_info<1, Block>::used_bits_mask)
		{
			count += bit_info<1, Block>::nr_elem_per_block;
			++sb.m_BlockData;
		}
		else
			do {
				if (bit_value<1>(*sb))
					++count;
				++sb;
			} while (sb.m_NrElems);
	}
	for (; sb != se; ++sb)
		if (bit_value<1>(*sb))
			++count;
	return count;
}

// *****************************************************************************
//                         Subset
// *****************************************************************************

template <typename ResContainer>
void make_subset_container(ResContainer* resultSub, const DataArray<Bool>* boolArray)
{
	dms_assert(resultSub);

	typedef DataArray<Bool>                ArgType;
	typedef Unit<typename ResContainer::value_type> ArgDomain;

	auto orgEntity = resultSub->GetValueRangeData();
	
	tile_write_channel<typename ResContainer::value_type> resDataChannel(resultSub);
	auto tn = orgEntity->GetNrTiles();
	for (tile_id t = 0; t!=tn; ++t)
	{
		auto resValuesRange = orgEntity->GetTileRange(t);
		auto boolData = boolArray->GetTile(t);

		dms_assert(!boolData.begin().m_NrElems);

		ArgType::const_iterator::block_type* di = boolData.begin().m_BlockData;
		ArgType::const_iterator::block_type* de = boolData.end  ().m_BlockData;

		SizeT count = 0;
		for (; di != de; ++di)
		{
			if (*di)
			{
				for (ArgType::const_iterator i(di, SizeT(0)), e(di, SizeT(ArgType::const_iterator::nr_elem_per_block)); i!=e; ++count, ++i)
					if (Bool(*i))
						resDataChannel.Write(Range_GetValue_naked(resValuesRange, count) );
			}
			else
				count += ArgType::const_iterator::nr_elem_per_block;
		}
		for (ArgType::const_iterator i(di, SizeT(0)), e=boolData.end(); i!=e; ++count, ++i)
			if (Bool(*i))
				resDataChannel.Write(Range_GetValue_naked(resValuesRange, count) );
	}
	dms_assert(resDataChannel.IsEndOfChannel());
}

static TokenID s_nrOrgEntity = GetTokenID_st("nr_OrgEntity");
static TokenID s_Org_rel = GetTokenID_st("org_rel");

enum class OrgRelCreationMode { none, org_rel, nr_OrgEntity };

struct SubsetOperator: public UnaryOperator
{
	using ArgType = DataArray<Bool>;

	OrgRelCreationMode m_ORCM;

	SubsetOperator(CommonOperGroup& cog, const Class* resDomainClass, OrgRelCreationMode orcm)
		: UnaryOperator(&cog, resDomainClass, DataArray<Bool>::GetStaticClass())
		, m_ORCM(orcm)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 1);

		const AbstrDataItem* arg1A = debug_cast<const AbstrDataItem*>(args[0]);
		dms_assert(arg1A);

		const AbstrUnit* arg1Domain = arg1A->GetAbstrDomainUnit();
		dms_assert(arg1Domain);

		const ValueClass* vc           = arg1Domain->GetValueType();
		const UnitClass*  resDomainCls = dynamic_cast<const UnitClass*>(m_ResultClass);
		if (!resDomainCls)
			resDomainCls = UnitClass::Find(vc->GetCrdClass());

		AbstrUnit* res  = resDomainCls->CreateResultUnit(resultHolder);
		dms_assert(res);
		resultHolder = res;

		AbstrDataItem* resSub = nullptr;
		if (m_ORCM != OrgRelCreationMode::none)
		{
			auto resSubName = (m_ORCM == OrgRelCreationMode::org_rel) ? s_Org_rel : s_nrOrgEntity;
			resSub = CreateDataItem(res, resSubName, res, arg1Domain);
			MG_PRECONDITION(resSub);
		}

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			Calculate(res, resSub, arg1A, arg1Domain);
		}
		return true;
	}

	void Calculate(AbstrUnit* res, 
		      AbstrDataItem* resSub, 
		const AbstrDataItem* arg1A,
		const AbstrUnit*     arg1Domain) const
	{
		const ArgType* arg1Obj  = const_array_cast<Bool>(arg1A);
		dms_assert(arg1Obj);

		tile_id tn = arg1Domain->GetNrTiles();

		ArgType::locked_cseq_t arg1RecycledLock; // use to avoid multiple locking in case of 1 tile only

		std::atomic<SizeT> count = 0;
		parallel_tileloop(tn, [=, &count, &arg1RecycledLock](tile_id t)
			{
				auto boolData = arg1Obj->GetTile(t);
				count += count_bitvalues(boolData.begin(), boolData.end());
				if (!t)
					arg1RecycledLock = std::move(boolData);
			}
		);

		res->SetCount(count);

		if (resSub)
		{
			DataWriteLock resSubLock(resSub);

			dms_assert(resSub->GetAbstrValuesUnit()->UnifyDomain(arg1A->GetAbstrDomainUnit()));

			visit<typelists::domain_elements>(arg1Domain, 
				[&resSubLock, arg1Obj] <typename a_type> (const Unit<a_type>*)
				{
					make_subset_container(
						mutable_array_cast<a_type>(resSubLock)
						, arg1Obj
					);
				}
			);

			resSubLock.Commit();
		}
	}
};

CommonOperGroup cog_subset_data("select_data");

struct AbstrSelectDataOperator : TernaryOperator
{
	AbstrSelectDataOperator(ClassCPtr dataClass)
		: TernaryOperator(&cog_subset_data, dataClass, AbstrUnit::GetStaticClass(), DataArray<Bool>::GetStaticClass(), dataClass)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 3);

		const AbstrUnit* subset = debug_cast<const AbstrUnit*>(args[0]);
		const AbstrDataItem* condA = debug_cast<const AbstrDataItem*>(args[1]);
		const AbstrDataItem* dataA = debug_cast<const AbstrDataItem*>(args[2]);
		condA->GetAbstrDomainUnit()->UnifyDomain(dataA->GetAbstrDomainUnit(), UM_Throw);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(subset, dataA->GetAbstrValuesUnit());
		if (mustCalc)
		{
			AbstrDataItem* res = debug_cast<AbstrDataItem*>(resultHolder.GetNew());

			DataReadLock arg1Lock(condA);
			DataReadLock arg2Lock(dataA);
			DataWriteLock resLock(res);

			Calculate(resLock, subset, condA, dataA);
			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(DataWriteHandle& res, const AbstrUnit* subset, const AbstrDataItem* condA, const AbstrDataItem* dataA) const = 0;
};

template <typename V>
struct SelectDataOperator : AbstrSelectDataOperator
{
	SelectDataOperator()
		: AbstrSelectDataOperator(DataArray<V>::GetStaticClass())
	{}

	void Calculate(DataWriteHandle& res, const AbstrUnit* subset, const AbstrDataItem* condA, const AbstrDataItem* dataA) const override
	{
		tile_write_channel<V> resDataChannel(mutable_array_cast<V>(res));

		const DataArray<Bool>* cond = const_array_cast<Bool>(condA);
		const DataArray<V   >* data = const_array_cast<V>   (dataA);

		tile_id tn = condA->GetAbstrDomainUnit()->GetNrTiles();

		for (tile_id t = 0; t != tn; ++t)
		{
//			ReadableTileLock condLock(cond, t), dataLock(data, t);

			auto boolData = cond->GetLockedDataRead(t);
			auto dataData = data->GetLockedDataRead(t);

			auto di = boolData.begin().m_BlockData;
			auto de = boolData.end().m_BlockData;

			SizeT count = 0;
			for (; di != de; ++di)
			{
				if (*di)
				{
					for (int i=0, e=DataArray<Bool>::const_iterator::nr_elem_per_block; i != e; ++count, ++i)
						if (Bool(*DataArray<Bool>::const_iterator(di, SizeT(i))))
							resDataChannel.Write(dataData[count]);
				}
				else
					count += DataArray<Bool>::const_iterator::nr_elem_per_block;
			}
			for (DataArray<Bool>::const_iterator i(di, SizeT(0)), e = boolData.end(); i != e; ++count, ++i)
				if (Bool(*i))
					resDataChannel.Write(dataData[count]);
		}
		MG_CHECK(resDataChannel.IsEndOfChannel());
	}
};

// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

namespace {
	CommonOperGroup cog_subset_xx("subset", oper_policy::dynamic_result_class);
	CommonOperGroup cog_subset_08("subset_uint8");
	CommonOperGroup cog_subset_16("subset_uint16");
	CommonOperGroup cog_subset_32("subset_uint32");

	CommonOperGroup cog_subset_Unit_xx("select_unit", oper_policy::dynamic_result_class);
	CommonOperGroup cog_subset_Unit_08("select_unit_uint8");
	CommonOperGroup cog_subset_Unit_16("select_unit_uint16");
	CommonOperGroup cog_subset_Unit_32("select_unit_uint32");

	CommonOperGroup cog_subset_orgrel_xx("select_orgrel", oper_policy::dynamic_result_class);
	CommonOperGroup cog_subset_orgrel_08("select_orgrel_uint8");
	CommonOperGroup cog_subset_orgrel_16("select_orgrel_uint16");
	CommonOperGroup cog_subset_orgrel_32("select_orgrel_uint32");

	SubsetOperator operXX(cog_subset_xx, AbstrUnit::GetStaticClass(), OrgRelCreationMode::nr_OrgEntity);
	SubsetOperator oper08(cog_subset_08, Unit<UInt8>::GetStaticClass(), OrgRelCreationMode::nr_OrgEntity);
	SubsetOperator oper16(cog_subset_16, Unit<UInt16>::GetStaticClass(), OrgRelCreationMode::nr_OrgEntity);
	SubsetOperator oper32(cog_subset_32, Unit<UInt32>::GetStaticClass(), OrgRelCreationMode::nr_OrgEntity);

	SubsetOperator operUnitXX(cog_subset_Unit_xx, AbstrUnit::GetStaticClass(), OrgRelCreationMode::none);
	SubsetOperator operUnit08(cog_subset_Unit_08, Unit<UInt8>::GetStaticClass(), OrgRelCreationMode::none);
	SubsetOperator operUnit16(cog_subset_Unit_16, Unit<UInt16>::GetStaticClass(), OrgRelCreationMode::none);
	SubsetOperator operUnit32(cog_subset_Unit_32, Unit<UInt32>::GetStaticClass(), OrgRelCreationMode::none);

	SubsetOperator operOrgRelXX(cog_subset_orgrel_xx, AbstrUnit::GetStaticClass(), OrgRelCreationMode::org_rel);
	SubsetOperator operOrgRel08(cog_subset_orgrel_08, Unit<UInt8>::GetStaticClass(), OrgRelCreationMode::org_rel);
	SubsetOperator operOrgRel16(cog_subset_orgrel_16, Unit<UInt16>::GetStaticClass(), OrgRelCreationMode::org_rel);
	SubsetOperator operOrgRel32(cog_subset_orgrel_32, Unit<UInt32>::GetStaticClass(), OrgRelCreationMode::org_rel);

	tl_oper::inst_tuple<typelists::value_elements, SelectDataOperator<_>> subsetDataOperInstances;
}

