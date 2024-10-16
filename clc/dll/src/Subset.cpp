// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "mci/CompositeCast.h"
#include "mci/ValueClass.h"
#include "utl/mySPrintF.h"

#include "DataArray.h"
#include "DataItemClass.h"
#include "LispTreeType.h"
#include "ParallelTiles.h"
#include "TileChannel.h"
#include "TreeItemClass.h"
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
//                               select, subset, select_with_org_rel
// *****************************************************************************

template <typename ResContainer>
void make_subset_container(ResContainer* resultSub, const DataArray<Bool>* boolArray)
{
	dms_assert(resultSub);

	using ArgType = DataArray<Bool>;
	using org_value_t = typename ResContainer::value_type;
	using ArgDomain = Unit< org_value_t>;
	using ArgTileType = range_or_void_data<org_value_t>;
	auto orgAbstrTiling = boolArray->GetTiledRangeData();
	auto orgTiling = checked_cast<const ArgTileType*>(orgAbstrTiling.get());

	tile_write_channel<org_value_t> resDataChannel(resultSub);
	auto tn = orgTiling->GetNrTiles();
	for (tile_id t = 0; t!=tn; ++t)
	{
		auto resValuesRange = orgTiling->GetTileRange(t);
		auto boolData = boolArray->GetTile(t);

		dms_assert(!boolData.begin().m_NrElems);

		ArgType::const_iterator::block_type* di = boolData.begin().m_BlockData;
		ArgType::const_iterator::block_type* de = boolData.end  ().m_BlockData;

		SizeT count = 0;
		for (; di != de; ++di)
		{
			if (*di) // process the bit-block
			{
				for (ArgType::const_iterator i(di, SizeT(0)), e(di, SizeT(ArgType::const_iterator::nr_elem_per_block)); i!=e; ++count, ++i)
					if (Bool(*i))
						resDataChannel.Write(Range_GetValue_naked(resValuesRange, count) );
			}
			else
				count += ArgType::const_iterator::nr_elem_per_block; // jump over the bit-block
		}
		// process the remaining bits
		for (ArgType::const_iterator i(di, SizeT(0)), e=boolData.end(); i!=e; ++count, ++i)
			if (Bool(*i))
				resDataChannel.Write(Range_GetValue_naked(resValuesRange, count) );
	}
	dms_assert(resDataChannel.IsEndOfChannel());
}

enum class OrgRelCreationMode { none, org_rel, nr_OrgEntity, org_rel_and_use_it };

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
		assert(res);
		res->SetTSF(TSF_Categorical);

		resultHolder = res;

		AbstrDataItem* resSub = nullptr;
		if (m_ORCM != OrgRelCreationMode::none)
		{
			auto resSubName = ((m_ORCM == OrgRelCreationMode::org_rel) || (m_ORCM == OrgRelCreationMode::org_rel_and_use_it)) ? token::org_rel : token::nr_OrgEntity;
			resSub = CreateDataItem(res, resSubName, res, arg1Domain);
			resSub->SetTSF(TSF_Categorical);

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
			DataWriteLock resSubLock(resSub, dms_rw_mode::write_only_all, arg1Domain->GetTiledRangeData());

			assert(resSub->GetAbstrValuesUnit()->UnifyDomain(arg1A->GetAbstrDomainUnit(), "values of resSub", "e1"));

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

// *****************************************************************************
//                               selet_with_attr_xxx
// *****************************************************************************

struct SelectMetaOperator : public BinaryOperator
{
	SelectMetaOperator(AbstrOperGroup& cog, const Class* resDomainClass, OrgRelCreationMode orcm, TokenID selectOper)
		: BinaryOperator(&cog, resDomainClass, TreeItem::GetStaticClass(), DataArray<Bool>::GetStaticClass())
		, m_ORCM(orcm)
		, m_SelectOper(selectOper)
	{}

	using ArgType = DataArray<Bool>;

	OrgRelCreationMode m_ORCM;
	TokenID            m_SelectOper;

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, OperationContext*, LispPtr metaCallArgs) const override
	{
		assert(args.size() == 1);

		const TreeItem* attrContainer = GetItem(args[0]);

		auto containerExpr = metaCallArgs.Left();
		auto conditionExpr = metaCallArgs.Right().Left();
		auto conditionExprStr = AsFLispSharedStr(conditionExpr, FormattingFlags::None);
		auto conditionCalc = AbstrCalculator::ConstructFromLispRef(resultHolder.GetOld(), conditionExpr, CalcRole::Other);
		MG_CHECK(conditionCalc);
		auto conditionDC = GetDC(conditionCalc);
		LispRef conditionKeyExpr;
		const AbstrDataItem* conditionA = nullptr;
		if (conditionDC)
		{
			conditionKeyExpr = conditionDC->GetLispRef();

			auto conditionItem = conditionDC->MakeResult();
			if (conditionDC->WasFailed(FR_MetaInfo))
				conditionDC->ThrowFail();
			MG_CHECK(conditionItem);

			conditionA = AsDynamicDataItem(conditionItem.get());
		}
		if (!conditionA)
			throwErrorD(GetGroup()->GetNameStr(), "condition expected as 2nd argument");

		const AbstrUnit* domain = conditionA->GetAbstrDomainUnit();
		assert(domain);

		const ValueClass* vc = domain->GetValueType();
		const UnitClass* resDomainCls = dynamic_cast<const UnitClass*>(m_ResultClass);
		if (!resDomainCls)
			resDomainCls = UnitClass::Find(vc->GetCrdClass());

		AbstrUnit* res = resDomainCls->CreateResultUnit(resultHolder); // does this set result to Failed when 
		assert(res);
		auto resExpr = ExprList(m_SelectOper, conditionKeyExpr);
		assert(!resExpr.EndP());
		auto resDC = GetOrCreateDataController(resExpr);
		assert(resDC);
		res->SetDC(resDC);
		resultHolder = res;

		TokenID resSubName;
		LispRef resSubExpr;
		if (m_ORCM != OrgRelCreationMode::none)
		{
			resSubName = ((m_ORCM == OrgRelCreationMode::org_rel) || (m_ORCM == OrgRelCreationMode::org_rel_and_use_it)) ? token::org_rel : token::nr_OrgEntity;
			if (m_ORCM == OrgRelCreationMode::org_rel_and_use_it)
				resSubExpr = slSubItemCall(resExpr, resSubName.AsStrRange());
		}
		for (auto subItem = attrContainer->GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
		{
			if (!IsDataItem(subItem))
				continue;
			auto subDataItem = AsDataItem(subItem);
			auto subDataID = subDataItem->GetID();
			if (subDataID == token::org_rel || subDataID == token::nr_OrgEntity)
				continue;
			subDataItem->UpdateMetaInfo();
			if (!domain->UnifyDomain(subDataItem->GetAbstrDomainUnit()))
				continue;
			auto resSub = CreateDataItem(res, subDataID, res, subDataItem->GetAbstrValuesUnit(), subDataItem->GetValueComposition());

			SharedStr selectExpr;
			if (m_ORCM == OrgRelCreationMode::org_rel_and_use_it)
				selectExpr = mySSPrintF("collect_by_org_rel(org_rel, scope(.., %s/%s))"
				,	containerExpr.GetSymbID()
				,	subDataID
				);
			else
				selectExpr = mySSPrintF("collect_by_cond(., scope(.., %s), scope(.., %s/%s))"
				,	conditionExprStr
				,	containerExpr.GetSymbID()
				,	subDataID
				);
			auto oldExpr = resSub->mc_Expr;
			if (!oldExpr.empty() && oldExpr != selectExpr)
			{
				auto msg = mySSPrintF("Cannot set calculation rule '%s' to selected attribute '%s' as it is already defined as '%s'", selectExpr, subDataID, oldExpr);
				throwErrorD(GetGroup()->GetNameID(), msg.c_str());
			}
			resSub->SetExpr(selectExpr);
		}
		res->SetIsInstantiated();
	}
};

// *****************************************************************************
//                               collect_by_cond, collect_by_org_rel
// *****************************************************************************

struct AbstrCollectByCondOperator : TernaryOperator
{
	AbstrCollectByCondOperator(AbstrOperGroup& aog, ClassCPtr dataClass)
		: TernaryOperator(&aog, dataClass, AbstrUnit::GetStaticClass(), DataArray<Bool>::GetStaticClass(), dataClass)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 3);

		const AbstrUnit* subset = AsUnit(args[0]);
		const AbstrDataItem* condA = AsDataItem(args[1]);
		const AbstrDataItem* dataA = AsDataItem(args[2]);
		condA->GetAbstrDomainUnit()->UnifyDomain(dataA->GetAbstrDomainUnit(), "e1", "e2", UM_Throw);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(subset, dataA->GetAbstrValuesUnit(), dataA->GetValueComposition());
		if (dataA->GetTSF(TSF_Categorical))
			resultHolder->SetTSF(TSF_Categorical);

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
struct CollectByCondOperator : AbstrCollectByCondOperator
{
	CollectByCondOperator(AbstrOperGroup& aog)
		: AbstrCollectByCondOperator(aog, DataArray<V>::GetStaticClass())
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

			auto di = boolData.begin().m_BlockData;
			auto de = boolData.end().m_BlockData;

			for (; di != de; ++di)
			{
				if (*di)
					goto processDataData;
			}
			for (DataArray<Bool>::const_iterator i(di, SizeT(0)), e = boolData.end(); i != e; ++i)
				if (Bool(*i))
					goto processDataData;
			continue; // go to next time

		processDataData:
			auto dataData = data->GetLockedDataRead(t);
			SizeT count = (di - boolData.begin().m_BlockData) * DataArray<Bool>::const_iterator::nr_elem_per_block;
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
//                               collect_with_attr_by_cond, collect_with_attr_by_org_rel
// *****************************************************************************

enum class collect_mode { org_rel, condition };

struct CollectWithAttrOperator : public BinaryOperator
{
	collect_mode m_CollectMode;
	CollectWithAttrOperator(AbstrOperGroup& cog, collect_mode collectMode)
		: BinaryOperator(&cog, TreeItem::GetStaticClass(), TreeItem::GetStaticClass(), AbstrUnit::GetStaticClass()) //, AbstrDataItem::GetStaticClass())
		, m_CollectMode(collectMode)
	{}

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, OperationContext*, LispPtr metaCallArgs) const override
	{
		assert(args.size() == 2);

		const TreeItem* attrContainer = GetItem(args[0]);

		auto subsetDomainItem = GetItem(args[1]);
		const AbstrUnit* domainA = AsDynamicUnit(subsetDomainItem);
		MG_USERCHECK2(domainA, "domain unit expected as 2nd argument");

		auto containerExpr = metaCallArgs.Left();
		auto subsetDomainExpr = metaCallArgs.Right().Left();
		auto subsetDomainExprStr = AsFLispSharedStr(subsetDomainExpr, FormattingFlags::None);

		MG_USERCHECK2(metaCallArgs.Right().Right().IsRealList(), m_CollectMode == collect_mode::org_rel
			? "collect_with_attr_by_org_rel: org_rel data-item expected as 3rd argument"
			: "condition data-item expected as 3rd argument"
		);
		const AbstrDataItem* condOrOrgRelA = nullptr;
		SharedStr condOrOrgRelExprStr;
		DataControllerRef condOrOrgRelDC;
		if (metaCallArgs.Right().Right().IsRealList())
		{
			auto condOrOrgRelExpr = metaCallArgs.Right().Right().Left();
			condOrOrgRelExprStr = AsFLispSharedStr(condOrOrgRelExpr, FormattingFlags::None);

			auto condOrOrgRelCalc = AbstrCalculator::ConstructFromLispRef(resultHolder.GetOld(), condOrOrgRelExpr, CalcRole::Other);
			condOrOrgRelDC = GetDC(condOrOrgRelCalc);
			MG_CHECK(condOrOrgRelDC);
			//		condOrOrgRelExpr = condOrOrgRelDC->GetLispRef();
			auto condOrOrgRelItem = condOrOrgRelDC->MakeResult();
			MG_CHECK(condOrOrgRelItem);

			condOrOrgRelA = AsDynamicDataItem(condOrOrgRelItem.get());
		}
		MG_USERCHECK2(condOrOrgRelA,
			m_CollectMode == collect_mode::org_rel
			? "collect_with_attr_by_org_rel: org_rel data-item expected as 3rd argument"
			: "collect_with_attr_cond: condition data-item expected as 3rd argument"
		);

		const AbstrUnit* sourceDomain = (m_CollectMode == collect_mode::org_rel) ? condOrOrgRelA->GetAbstrValuesUnit() : condOrOrgRelA->GetAbstrDomainUnit();
		assert(sourceDomain);
		assert(resultHolder);
		if (m_CollectMode == collect_mode::org_rel)
			MG_USERCHECK2(domainA->UnifyDomain(condOrOrgRelA->GetAbstrDomainUnit()), "collect_with_attr_by_org_rel(attr_container, subset_domain, org_rel): target_domain doesn't match the domain of org_rel");

		for (auto subItem = attrContainer->GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
		{
			if (!IsDataItem(subItem))
				continue;
			auto subDataItem = AsDataItem(subItem);
			auto subDataID = subDataItem->GetID();
			if (subDataID == token::org_rel || subDataID == token::nr_OrgEntity)
				continue;
			subDataItem->UpdateMetaInfo();
			if (!sourceDomain->UnifyDomain(subDataItem->GetAbstrDomainUnit()))
			{
				if (m_CollectMode == collect_mode::org_rel)
					reportF(SeverityTypeID::ST_Warning, "%s: image of org_rel is %s, which is incompatible with the domain of attribute %s, which is %s"
					,	GetGroup()->GetNameStr()
					,	sourceDomain->GetFullCfgName().c_str()
					,	subDataItem->GetFullName().c_str()
					,	subDataItem->GetAbstrDomainUnit()->GetFullCfgName().c_str()
					);
				else
					reportF(SeverityTypeID::ST_Warning, "%s: domain of condition is %s, which is incompatible with the domain of attribute %s, which is %s"
					,	GetGroup()->GetNameStr()
					,	sourceDomain->GetFullCfgName().c_str()
					,	subDataItem->GetFullName().c_str()
					,	subDataItem->GetAbstrDomainUnit()->GetFullCfgName().c_str()
					);
				continue;
			}
			auto resSub = CreateDataItem(resultHolder, subDataID, domainA, subDataItem->GetAbstrValuesUnit(), subDataItem->GetValueComposition());

			SharedStr collectExpr;
			if (m_CollectMode == collect_mode::org_rel)
				collectExpr = mySSPrintF("scope(.., lookup(%s, %s/%s))"
					, condOrOrgRelExprStr
					, containerExpr.GetSymbID()
					, subDataID
				);
			else
				collectExpr = mySSPrintF("scope(.., collect_by_cond(%s, %s, %s/%s))"
					, subsetDomainExprStr
					, condOrOrgRelExprStr
					, containerExpr.GetSymbID()
					, subDataID
				);

			auto oldExpr = resSub->mc_Expr;
			if (!oldExpr.empty() && oldExpr != collectExpr)
			{
				auto msg = mySSPrintF("Cannot set calculation rule '%s' to collected attribute '%s' as it is already defined as '%s'", collectExpr, subDataID, oldExpr);
				throwErrorD(GetGroup()->GetNameID(), msg.c_str());
			}
			resSub->SetExpr(collectExpr);
		}
		resultHolder->SetIsInstantiated();
	}
};

// *****************************************************************************
// recollect_by_cond   (cond:    D->B, subset_attr: S->V, fillerValue: ->V) -> (D -> V)
// recollect_by_org_rel(org_rel: S->D, subset_attr: S->V, fillerValue: ->V) -> (D -> V)
// *****************************************************************************

struct AbstrRecollectByCondOperator : BinaryOperator
{
	AbstrRecollectByCondOperator(AbstrOperGroup& aog, ClassCPtr valuesClass)
		: BinaryOperator(&aog, valuesClass, DataArray<Bool>::GetStaticClass(), valuesClass)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		MG_USERCHECK2(args.size() == 2 || args.size() == 3, "recollect_by_cond: 2 or 3 arguments expected");

		const AbstrDataItem* condA = AsDataItem(args[0]);
		const AbstrDataItem* dataA = AsDataItem(args[1]);
		const AbstrDataItem* fillA = nullptr;
		if (args.size() == 3)
		{
			fillA = AsDynamicDataItem(args[2]);
			MG_USERCHECK2(fillA, "recollect_by_cond: third argument is expected to be an attribute or parameter");
			dataA->GetAbstrValuesUnit()->UnifyValues(fillA->GetAbstrValuesUnit(), "v2", "v3", UnifyMode::UM_Throw | UnifyMode::UM_AllowDefaultRight);
			condA->GetAbstrDomainUnit()->UnifyDomain(fillA->GetAbstrDomainUnit(), "e1", "e3", UnifyMode::UM_Throw | UnifyMode::UM_AllowDefaultRight | UnifyMode::UM_AllowVoidRight);
		}

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(condA->GetAbstrDomainUnit(), dataA->GetAbstrValuesUnit(), dataA->GetValueComposition());

		if (dataA->GetTSF(TSF_Categorical) || fillA && fillA->GetTSF(TSF_Categorical))
		{
			if (fillA)
				dataA->GetAbstrValuesUnit()->UnifyDomain(fillA->GetAbstrValuesUnit(), "v2", "v3", UnifyMode(UM_AllowDefaultRight | UM_Throw));
			resultHolder->SetTSF(TSF_Categorical);
		}
		if (mustCalc)
		{
			AbstrDataItem* res = debug_cast<AbstrDataItem*>(resultHolder.GetNew());

			DataReadLock arg1Lock(condA);
			DataReadLock arg2Lock(dataA);
			DataReadLock arg3Lock(fillA);
			DataWriteLock resLock(res);

			Calculate(resLock, condA, dataA, fillA);
			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(DataWriteHandle& res, const AbstrDataItem* condA, const AbstrDataItem* dataA, const AbstrDataItem* fillA) const = 0;
};


template <typename V>
struct RecollectByCondOperator : AbstrRecollectByCondOperator
{
	RecollectByCondOperator(AbstrOperGroup& aog)
		: AbstrRecollectByCondOperator(aog, DataArray<V>::GetStaticClass())
	{}

	void Calculate(DataWriteHandle& resH, const AbstrDataItem* condA, const AbstrDataItem* dataA, const AbstrDataItem* fillA) const override
	{
		const DataArray<Bool>* cond = const_array_cast<Bool>(condA);
		const DataArray<V   >* data = const_array_cast<V>   (dataA);
		const DataArray<V   >* fill = fillA ? const_array_cast<V>(fillA) : nullptr;

		V fillValue = (fillA && fillA->HasVoidDomainGuarantee())
			? fill->GetIndexedValue(0)
			: UNDEFINED_OR_ZERO(V);

		auto res = mutable_array_cast<V>(resH);

		auto valueReader = tile_read_channel<V>(data);

		tile_id tn = condA->GetAbstrDomainUnit()->GetNrTiles();

		for (tile_id t = 0; t != tn; ++t)
		{
			//			ReadableTileLock condLock(cond, t), dataLock(data, t);
			auto boolData = cond->GetTile(t);
			auto resData = res->GetWritableTile(t);
			auto resPtr = resData.begin();

			if (!fillA || fillA->HasVoidDomainGuarantee())
			{
				for (auto boolPtr = boolData.begin(), boolEnd = boolData.end(); boolPtr != boolEnd; ++resPtr, ++boolPtr)
				{
					if (Bool(*boolPtr))
					{
						MG_USERCHECK2(!valueReader.AtEnd(), "recollect_by_cond: number of trues in cond is greater than the number of values in the 2nd arguement. Attributues on select_by_cond with this condition are expected to match the number of elements.");
						*resPtr = *valueReader;
						++valueReader;
					}
					else
						*resPtr = fillValue;
				}
			}
			else
			{
				auto fillData = fill->GetTile(t);
				auto fillPtr = fillData.begin();
				for (auto boolPtr = boolData.begin(), boolEnd = boolData.end(); boolPtr != boolEnd; ++fillPtr, ++resPtr, ++boolPtr)
				{
					if (Bool(*boolPtr))
					{
						MG_USERCHECK2(!valueReader.AtEnd(), "recollect_by_cond: number of trues in cond is greater than the number of values in the 2nd arguement. Attributues on select_by_cond with this condition are expected to match the number of elements.");
						*resPtr = *valueReader;
						++valueReader;
					}
					else
						*resPtr = *fillPtr;
				}

			}
		}
		MG_USERCHECK2(valueReader.AtEnd(), "recollect_by_cond: number of trues in cond is less than the number of values in the 2nd arguement. Attributues on select_by_cond with this condition are expected to match the number of elements.");
	}
};

// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

#include "LispTreeType.h"

namespace {

	CommonOperGroup cog_select(token::select, oper_policy::dynamic_result_class);
	CommonOperGroup cog_select_08(token::select_uint8);
	CommonOperGroup cog_select_16(token::select_uint16);
	CommonOperGroup cog_select_32(token::select_uint32);
	CommonOperGroup cog_select_64(token::select_uint64);

	CommonOperGroup cog_select_with_org_rel(token::select_with_org_rel, oper_policy::dynamic_result_class);
	CommonOperGroup cog_select_08_with_org_rel(token::select_uint8_with_org_rel);
	CommonOperGroup cog_select_16_with_org_rel(token::select_uint16_with_org_rel);
	CommonOperGroup cog_select_32_with_org_rel(token::select_uint32_with_org_rel);
	CommonOperGroup cog_select_64_with_org_rel(token::select_uint64_with_org_rel);

	// Partly DEPRECIATED VARIANTS of select BEGIN
	CommonOperGroup cog_subset_xx("subset", oper_policy::dynamic_result_class);
	Obsolete<CommonOperGroup> cog_subset_08("use select_uint8_with_org_rel", "subset_uint8", oper_policy::depreciated);
	Obsolete<CommonOperGroup> cog_subset_16("use select_uint16_with_org_rel", "subset_uint16", oper_policy::depreciated);
	Obsolete<CommonOperGroup> cog_subset_32("use select_uint32_with_org_rel", "subset_uint32", oper_policy::depreciated);

	Obsolete<CommonOperGroup> cog_subset_Unit_xx("use select", token::select_unit, oper_policy::dynamic_result_class | oper_policy::depreciated);
	Obsolete<CommonOperGroup> cog_subset_Unit_08("use select_uint8", token::select_unit_uint8, oper_policy::depreciated);
	Obsolete<CommonOperGroup> cog_subset_Unit_16("use select_uint16", token::select_unit_uint16, oper_policy::depreciated);
	Obsolete<CommonOperGroup> cog_subset_Unit_32("use select_uint32", token::select_unit_uint32, oper_policy::depreciated);

	Obsolete<CommonOperGroup> cog_subset_orgrel_xx("use select_with_org_rel", token::select_orgrel, oper_policy::dynamic_result_class | oper_policy::depreciated);
	Obsolete<CommonOperGroup> cog_subset_orgrel_08("use select_uint8_with_org_rel", token::select_orgrel_uint8, oper_policy::depreciated);
	Obsolete<CommonOperGroup> cog_subset_orgrel_16("use select_uint16_with_org_rel", token::select_orgrel_uint16, oper_policy::depreciated);
	Obsolete<CommonOperGroup> cog_subset_orgrel_32("use select_uint32_with_org_rel", token::select_orgrel_uint32, oper_policy::depreciated);
	// Partly DEPRECIATED VARIANTS of select END


	oper_arg_policy oap_select_with_attr[2] = { oper_arg_policy::calc_never , oper_arg_policy::calc_as_result };

	SpecialOperGroup cog_select_with_attr_by_cond   (token::select_with_attr_by_cond       , 2, oap_select_with_attr, oper_policy::dynamic_result_class | oper_policy::dont_cache_result);
	SpecialOperGroup cog_select_08_with_attr_by_cond(token::select_uint8_with_attr_by_cond , 2, oap_select_with_attr, oper_policy::dont_cache_result);
	SpecialOperGroup cog_select_16_with_attr_by_cond(token::select_uint16_with_attr_by_cond, 2, oap_select_with_attr, oper_policy::dont_cache_result);
	SpecialOperGroup cog_select_32_with_attr_by_cond(token::select_uint32_with_attr_by_cond, 2, oap_select_with_attr, oper_policy::dont_cache_result);
	SpecialOperGroup cog_select_64_with_attr_by_cond(token::select_uint64_with_attr_by_cond, 2, oap_select_with_attr, oper_policy::dont_cache_result);

	SpecialOperGroup cog_select_with_org_rel_with_attr_by_cond   (token::select_with_org_rel_with_attr_by_cond       , 2, oap_select_with_attr, oper_policy::dynamic_result_class | oper_policy::dont_cache_result);
	SpecialOperGroup cog_select_08_with_org_rel_with_attr_by_cond(token::select_uint8_with_org_rel_with_attr_by_cond , 2, oap_select_with_attr, oper_policy::dont_cache_result);
	SpecialOperGroup cog_select_16_with_org_rel_with_attr_by_cond(token::select_uint16_with_org_rel_with_attr_by_cond, 2, oap_select_with_attr, oper_policy::dont_cache_result);
	SpecialOperGroup cog_select_32_with_org_rel_with_attr_by_cond(token::select_uint32_with_org_rel_with_attr_by_cond, 2, oap_select_with_attr, oper_policy::dont_cache_result);
	SpecialOperGroup cog_select_64_with_org_rel_with_attr_by_cond(token::select_uint64_with_org_rel_with_attr_by_cond, 2, oap_select_with_attr, oper_policy::dont_cache_result);

	SpecialOperGroup cog_select_with_attr_by_org_rel   (token::select_with_attr_by_org_rel       , 2, oap_select_with_attr, oper_policy::dynamic_result_class | oper_policy::dont_cache_result);
	SpecialOperGroup cog_select_08_with_attr_by_org_rel(token::select_uint8_with_attr_by_org_rel , 2, oap_select_with_attr, oper_policy::dont_cache_result);
	SpecialOperGroup cog_select_16_with_attr_by_org_rel(token::select_uint16_with_attr_by_org_rel, 2, oap_select_with_attr, oper_policy::dont_cache_result);
	SpecialOperGroup cog_select_32_with_attr_by_org_rel(token::select_uint32_with_attr_by_org_rel, 2, oap_select_with_attr, oper_policy::dont_cache_result);
	SpecialOperGroup cog_select_64_with_attr_by_org_rel(token::select_uint64_with_attr_by_org_rel, 2, oap_select_with_attr, oper_policy::dont_cache_result);

	// DEPRECIATED VARIANTS of select_attr BEGIN
	Obsolete < SpecialOperGroup> cog_subset_m_xx("select_with_attr_by_cond", token::select_many, 2, oap_select_with_attr, oper_policy::dynamic_result_class | oper_policy::dont_cache_result | oper_policy::depreciated);
	Obsolete < SpecialOperGroup> cog_subset_m_08("select_uint8_with_attr_by_cond", token::select_many_uint8, 2, oap_select_with_attr, oper_policy::dont_cache_result | oper_policy::depreciated);
	Obsolete < SpecialOperGroup> cog_subset_m_16("select_uint16_with_attr_by_cond", token::select_many_uint16, 2, oap_select_with_attr, oper_policy::dont_cache_result | oper_policy::depreciated);
	Obsolete < SpecialOperGroup> cog_subset_m_32("select_uint32_with_attr_by_cond", token::select_many_uint32, 2, oap_select_with_attr, oper_policy::dont_cache_result | oper_policy::depreciated);
	Obsolete < SpecialOperGroup> cog_subset_a_xx("select_with_attr_by_org_rel", token::select_afew       , 2, oap_select_with_attr, oper_policy::dynamic_result_class | oper_policy::dont_cache_result | oper_policy::depreciated);
	Obsolete < SpecialOperGroup> cog_subset_a_08("select_uint8_with_attr_by_org_rel", token::select_afew_uint8 , 2, oap_select_with_attr, oper_policy::dont_cache_result | oper_policy::depreciated);
	Obsolete < SpecialOperGroup> cog_subset_a_16("select_uint16_with_attr_by_org_rel", token::select_afew_uint16, 2, oap_select_with_attr, oper_policy::dont_cache_result | oper_policy::depreciated);
	Obsolete < SpecialOperGroup> cog_subset_a_32("select_uint32_with_attr_by_org_rel", token::select_afew_uint32, 2, oap_select_with_attr, oper_policy::dont_cache_result | oper_policy::depreciated);
	// DEPRECIATED VARIANTS of select_attr END

	SubsetOperator operSXX(cog_select, AbstrUnit::GetStaticClass(), OrgRelCreationMode::none);
	SubsetOperator operS08(cog_select_08, Unit<UInt8>::GetStaticClass(), OrgRelCreationMode::none);
	SubsetOperator operS16(cog_select_16, Unit<UInt16>::GetStaticClass(), OrgRelCreationMode::none);
	SubsetOperator operS32(cog_select_32, Unit<UInt32>::GetStaticClass(), OrgRelCreationMode::none);
	SubsetOperator operS64(cog_select_64, Unit<UInt64>::GetStaticClass(), OrgRelCreationMode::none);

	SubsetOperator operSORXX(cog_select_with_org_rel, AbstrUnit::GetStaticClass(), OrgRelCreationMode::org_rel);
	SubsetOperator operSOR08(cog_select_08_with_org_rel, Unit<UInt8>::GetStaticClass(), OrgRelCreationMode::org_rel);
	SubsetOperator operSOR16(cog_select_16_with_org_rel, Unit<UInt16>::GetStaticClass(), OrgRelCreationMode::org_rel);
	SubsetOperator operSOR32(cog_select_32_with_org_rel, Unit<UInt32>::GetStaticClass(), OrgRelCreationMode::org_rel);
	SubsetOperator operSOR64(cog_select_64_with_org_rel, Unit<UInt64>::GetStaticClass(), OrgRelCreationMode::org_rel);

	// old subset BEGIN
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
	// old subset END


	SelectMetaOperator operMetaSMxx(cog_select_with_attr_by_cond, AbstrUnit::GetStaticClass(), OrgRelCreationMode::none, token::select);
	SelectMetaOperator operMetaSM08(cog_select_08_with_attr_by_cond, Unit<UInt8>::GetStaticClass(), OrgRelCreationMode::none, token::select_uint8);
	SelectMetaOperator operMetaSM16(cog_select_16_with_attr_by_cond, Unit<UInt16>::GetStaticClass(), OrgRelCreationMode::none, token::select_uint16);
	SelectMetaOperator operMetaSM32(cog_select_32_with_attr_by_cond, Unit<UInt32>::GetStaticClass(), OrgRelCreationMode::none, token::select_uint32);
	SelectMetaOperator operMetaSM64(cog_select_64_with_attr_by_cond, Unit<UInt64>::GetStaticClass(), OrgRelCreationMode::none, token::select_uint64);

	SelectMetaOperator operMetaCAxx(cog_select_with_org_rel_with_attr_by_cond, AbstrUnit::GetStaticClass(), OrgRelCreationMode::org_rel, token::select_with_org_rel);
	SelectMetaOperator operMetaCA08(cog_select_08_with_org_rel_with_attr_by_cond, Unit<UInt8>::GetStaticClass(), OrgRelCreationMode::org_rel, token::select_uint8_with_org_rel);
	SelectMetaOperator operMetaCA16(cog_select_16_with_org_rel_with_attr_by_cond, Unit<UInt16>::GetStaticClass(), OrgRelCreationMode::org_rel, token::select_uint16_with_org_rel);
	SelectMetaOperator operMetaCA32(cog_select_32_with_org_rel_with_attr_by_cond, Unit<UInt32>::GetStaticClass(), OrgRelCreationMode::org_rel, token::select_uint32_with_org_rel);
	SelectMetaOperator operMetaCA64(cog_select_64_with_org_rel_with_attr_by_cond, Unit<UInt64>::GetStaticClass(), OrgRelCreationMode::org_rel, token::select_uint64_with_org_rel);

	SelectMetaOperator operMetaSAxx(cog_select_with_attr_by_org_rel   , AbstrUnit::GetStaticClass(), OrgRelCreationMode::org_rel_and_use_it, token::select_with_org_rel);
	SelectMetaOperator operMetaSA08(cog_select_08_with_attr_by_org_rel, Unit<UInt8>::GetStaticClass(), OrgRelCreationMode::org_rel_and_use_it, token::select_uint8_with_org_rel);
	SelectMetaOperator operMetaSA16(cog_select_16_with_attr_by_org_rel, Unit<UInt16>::GetStaticClass(), OrgRelCreationMode::org_rel_and_use_it, token::select_uint16_with_org_rel);
	SelectMetaOperator operMetaSA32(cog_select_32_with_attr_by_org_rel, Unit<UInt32>::GetStaticClass(), OrgRelCreationMode::org_rel_and_use_it, token::select_uint32_with_org_rel);
	SelectMetaOperator operMetaSA64(cog_select_64_with_attr_by_org_rel, Unit<UInt64>::GetStaticClass(), OrgRelCreationMode::org_rel_and_use_it, token::select_uint64_with_org_rel);

	// DEPRECIATED VARIANTS of select_attr BEGIN
	SelectMetaOperator operMetaMxx(cog_subset_m_xx, AbstrUnit::GetStaticClass(), OrgRelCreationMode::none, token::select);
	SelectMetaOperator operMetaM08(cog_subset_m_08, Unit<UInt8>::GetStaticClass(), OrgRelCreationMode::none, token::select_uint8);
	SelectMetaOperator operMetaM16(cog_subset_m_16, Unit<UInt16>::GetStaticClass(), OrgRelCreationMode::none, token::select_uint16);
	SelectMetaOperator operMetaM32(cog_subset_m_32, Unit<UInt32>::GetStaticClass(), OrgRelCreationMode::none, token::select_uint32);

	SelectMetaOperator operMetaAxx(cog_subset_a_xx, AbstrUnit::GetStaticClass(), OrgRelCreationMode::org_rel_and_use_it, token::select_with_org_rel);
	SelectMetaOperator operMetaA08(cog_subset_a_08, Unit<UInt8>::GetStaticClass(), OrgRelCreationMode::org_rel_and_use_it, token::select_uint8_with_org_rel);
	SelectMetaOperator operMetaA16(cog_subset_a_16, Unit<UInt16>::GetStaticClass(), OrgRelCreationMode::org_rel_and_use_it, token::select_uint16_with_org_rel);
	SelectMetaOperator operMetaA32(cog_subset_a_32, Unit<UInt32>::GetStaticClass(), OrgRelCreationMode::org_rel_and_use_it, token::select_uint32_with_org_rel);
	// DEPRECIATED VARIANTS of select_attr END

	oper_arg_policy oap_Relate[3] = { oper_arg_policy::calc_never , oper_arg_policy::calc_never, oper_arg_policy::calc_at_subitem };

	SpecialOperGroup sog_collect_attr_by_org_rel(token::collect_attr_by_org_rel, 3, oap_Relate, oper_policy::dont_cache_result);
	SpecialOperGroup sog_collect_attr_by_cond   (token::collect_attr_by_cond   , 3, oap_Relate, oper_policy::dont_cache_result);
	CollectWithAttrOperator operCF(sog_collect_attr_by_org_rel, collect_mode::org_rel);
	CollectWithAttrOperator operCM(sog_collect_attr_by_cond, collect_mode::condition);

	// DEPRECIATED VARIANTS of collect_attr BEGIN
	Obsolete<SpecialOperGroup> cog_relate_afew("use collect_attr_by_org_rel", "relate_afew", 3, oap_Relate, oper_policy::dont_cache_result | oper_policy::obsolete);
	Obsolete<SpecialOperGroup> cog_relate_attr("use collect_attr_by_org_rel", "relate_attr", 3, oap_Relate, oper_policy::dont_cache_result | oper_policy::obsolete);
	Obsolete<SpecialOperGroup> cog_relate_many("use collect_attr_by_cond", "relate_many", 3, oap_Relate, oper_policy::dont_cache_result | oper_policy::obsolete);

	CollectWithAttrOperator operRF(cog_relate_afew, collect_mode::org_rel);
	CollectWithAttrOperator operRA(cog_relate_attr, collect_mode::org_rel);
	CollectWithAttrOperator operRM(cog_relate_many, collect_mode::condition);
	// DEPRECIATED VARIANTS of collect_attr END

	Obsolete<CommonOperGroup> cog_select_data("use collect_by_cond", "select_data", oper_policy::obsolete);
	CommonOperGroup cog_collect_by_cond(token::collect_by_cond);
	CommonOperGroup cog_recollect_by_cond(token::recollect_by_cond, oper_policy::allow_extra_args);

	tl_oper::inst_tuple_templ<typelists::value_elements, CollectByCondOperator, AbstrOperGroup&> selectDataOperInstances(cog_select_data);
	tl_oper::inst_tuple_templ<typelists::value_elements, CollectByCondOperator, AbstrOperGroup&> collectByCondOperInstances(cog_collect_by_cond);
	tl_oper::inst_tuple_templ<typelists::value_elements, RecollectByCondOperator, AbstrOperGroup&> recollectByCondOperInstances(cog_recollect_by_cond);
}

