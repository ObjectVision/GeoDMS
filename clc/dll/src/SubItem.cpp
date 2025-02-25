// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include <future>

#include "act/any.h"

#include "dbg/SeverityType.h"
#include "CheckedDomain.h"
#include "OperGroups.h"
#include "TreeItemClass.h"

// *****************************************************************************
//										SubItemOperator
// *****************************************************************************

oper_arg_policy oap_SubItem[2] = { oper_arg_policy::calc_subitem_root, oper_arg_policy::calc_always };

SpecialOperGroup sog_SubItem("SubItem", 2, oap_SubItem, oper_policy::existing|oper_policy::dynamic_result_class);

struct SubItemOperator: BinaryOperator
{
	using Arg2Type = DataArray<SharedStr>;

	SubItemOperator()
		: BinaryOperator(&sog_SubItem, TreeItem::GetStaticClass(), TreeItem::GetStaticClass(), Arg2Type::GetStaticClass())
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 2);
		const TreeItem* arg1 = args[0];
		assert(arg1);
		assert(arg1->IsCacheItem());
		if (!resultHolder) {
			dms_assert(!mustCalc);
			checked_domain<Void>(args[1], "a2");

			SharedStr subItemName = GetCurrValue<SharedStr>(args[1], 0);
			const TreeItem* subItem = arg1->GetCurrItem(subItemName);

			if (!subItem)
				GetGroup()->throwOperErrorF("Cannot find '%s' from '%s'",
					subItemName.c_str(),
					arg1->GetFullName().c_str()
				);
			assert(subItem->IsCacheItem());
			resultHolder = subItem;
		}
		assert(resultHolder);

		if (mustCalc)
		{
			assert(CheckDataReady(arg1->GetCurrUltimateItem()));
			SharedTreeItem ultItem = resultHolder.GetUlt();
			assert(CheckDataReady(ultItem));
			if (ultItem != resultHolder.GetOld() && IsDataReady(ultItem))
			{
				SharedTreeItem ultHolder = resultHolder.GetUlt();
				resultHolder->m_ReadAssets.emplace<SharedTreeItemInterestPtr>(std::move(ultHolder));
			}
			resultHolder->SetIsInstantiated();
		}
		return true;
	}
};

// *****************************************************************************
//										CheckOperator
// *****************************************************************************

#include "utl/mySPrintF.h"
#include "LispTreeType.h"
#include "TicPropDefConst.h"

//oper_arg_policy oap_SubItem[2] = { oper_arg_policy::calc_subitem_root, oper_arg_policy::calc_always };

CommonOperGroup sog_Check(token::integrity_check, oper_policy::existing|oper_policy::dynamic_result_class);

struct CheckOperator : public BinaryOperator
{
	using Arg2Type = DataArray<Bool>;

	CheckOperator()
		: BinaryOperator(&sog_Check,
			TreeItem::GetStaticClass(),
			TreeItem::GetStaticClass(), Arg2Type::GetStaticClass())
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 2);
		const TreeItem* arg1 = args[0];
		assert(arg1);
//		dms_assert(arg1->IsCacheItem());
		if (!resultHolder) {
			dms_assert(!mustCalc);
			resultHolder = arg1;
		}
		assert(resultHolder);

		if (mustCalc)
		{
			auto arg2A = AsDataItem(args[1]);
			dms_assert(CheckDataReady(arg2A));
			dms_assert(CheckDataReady(resultHolder.GetOld()));
			DataReadLock arg2Lock(arg2A);
			SizeT nrFailures = arg2A->CountValues<Bool>(false);
			if (nrFailures)
			{
				auto ultimate_item = resultHolder.GetUlt();
				// will be caught by SuspendibleUpdate who will Fail this.
				resultHolder.Fail(mySSPrintF( "[[%s]] %s : %d element(s) failed", ultimate_item ? ultimate_item->GetFullCfgName().c_str() : "", ICHECK_NAME, nrFailures), FR_Validate); // will be caught by SuspendibleUpdate who will Fail this.
				assert(resultHolder.WasFailed(FR_Validate));
			}
		}
		return true;
	}
};

// *****************************************************************************
//										FenceOperator
// *****************************************************************************

#include "CopyTreeContext.h"
#include "OperationContext.h"
#include "UnitProcessor.h"

oper_arg_policy oap_Fence[2] = { oper_arg_policy::subst_with_subitems,  oper_arg_policy::calc_as_result };
SpecialOperGroup sog_FenceContainer(token::FenceContainer, 2, oap_Fence, oper_policy::dynamic_result_class);

struct FenceContainerOperator : BinaryOperator
{
	FenceContainerOperator()
		: BinaryOperator(&sog_FenceContainer, TreeItem::GetStaticClass(), TreeItem::GetStaticClass(), DataArray<SharedStr>::GetStaticClass())
	{}

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, OperationContext* fc, LispPtr) const override
	{
		assert(args.size() == 2);
		assert(fc);

		auto sourceContainer = std::get<SharedTreeItem>(args[0]).get();
		if (!resultHolder)
		{
			CopyTreeContext context(nullptr, sourceContainer, ""
				, DataCopyMode::MakePassor | DataCopyMode::MakeEndogenous | DataCopyMode::InFenceOperator | DataCopyMode::NoRoot //| DataCopyMode::CopyReferredItems
			);
			context.m_FenceNumber = GetNextFenceNumber();

			resultHolder = context.Apply();
			resultHolder->m_FenceNumber = context.m_FenceNumber;
			resultHolder.m_FenceNumber = context.m_FenceNumber;
		}
		assert(resultHolder);

		auto resultFenceNumer = resultHolder.m_FenceNumber;

		auto resultRoot = resultHolder.GetNew();
		for (auto resWalker = resultRoot; resWalker; resWalker = resultRoot->WalkCurrSubTree(resWalker))
		{
			MG_CHECK(resWalker->m_FenceNumber >= resultFenceNumer);
		
			auto srcItem = sourceContainer->FindItem(resWalker->GetRelativeName(resultHolder.GetNew()));
			MG_CHECK(!srcItem->IsCacheItem());

			if (srcItem->WasFailed())
				resWalker->Fail(srcItem);
			if(!resWalker->WasFailed(FR_MetaInfo))
			{
				if (srcItem->mc_DC)
					fc->AddDependency(srcItem->GetCheckedDC());
				resWalker->SetReferredItem(srcItem);
			}
		}
//		resultHolder->m_ReadAssets.emplace<fence_work_data>(std::move(workData));
//		assert(resultHolder->m_ReadAssets.has_value());
	}
	bool CalcResult(TreeItemDualRef& resultHolder, ArgRefs args, std::vector<ItemReadLock> readLocks, OperationContext * fc, Explain::Context * context) const override
	{
		MG_CHECK(!resultHolder->GetIsInstantiated());

		assert(args.size() == 2);

		DataReadLock msgLock(AsDataItem(args[1]));
		auto msgData = const_array_cast<SharedStr>(msgLock)->GetDataRead();

		reportD(SeverityTypeID::ST_MinorTrace, "FenceContainer START");
		if (msgData.size() != 1 || !msgData[0].empty())
			for (auto msg : msgData)
				reportD(SeverityTypeID::ST_MinorTrace, msg.AsRange());

		auto srcContainer = std::get<SharedTreeItem>(args[0]).get();

//		assert(resultHolder->m_ReadAssets.has_value());

//		const fence_work_data* workDataPtr = rtc::any::any_cast<fence_work_data>(&resultHolder->m_ReadAssets);

//		MG_CHECK(workDataPtr);

		// first, copy ranges of units ?
//		auto resultRoot = resultHolder.GetNew();
//		assert(resultRoot);

		using fence_member_pair = std::pair<SharedPtr<const TreeItem>, FutureData>;
		using fence_work_data = std::vector<fence_member_pair>;

		fence_work_data futureDataContainer;

		auto resultFenceNumer = resultHolder.m_FenceNumber;

		std::promise<void> fenceBell;
		auto bellWaiter = fenceBell.get_future();
		auto srcWalker = srcContainer;
		
		reportD(SeverityTypeID::ST_MinorTrace, "FenceContainer ResultFutures collected");
		if (msgData.size() != 1 || !msgData[0].empty())
			for (auto msg : msgData)
				reportD(SeverityTypeID::ST_MinorTrace, msg.AsRange());


//		std::vector<SharedTreeItemInterestPtr> interestHolders;

		PostMainThreadTask([srcContainer, &srcWalker, &fenceBell, &resultHolder, resultFenceNumer, &futureDataContainer](bool mustCancel)-> bool
			{
				assert(!SuspendTrigger::BlockerBase::IsBlocked());
				// work on exporting stuff from main thread
				if (!mustCancel)
				{
					for (; srcWalker; srcWalker = srcContainer->WalkConstSubTree(srcWalker))
					{
						if (srcWalker->WasFailed(FR_MetaInfo))
							continue;

						MG_CHECK(srcWalker->GetCurrRangeItem()->HasInterest());
						MG_CHECK(srcWalker->m_FenceNumber < resultFenceNumer);

//						if (interestHolders.empty() || interestHolders.back() != srcWalker)
//							interestHolders.emplace_back(srcWalker);

						if (!srcWalker->SuspendibleUpdate(PS_Committed))
						{
							if (srcWalker->WasFailed())
								resultHolder.GetNew()->Fail(srcWalker);
							if (SuspendTrigger::DidSuspend())
								return false;
						}

						if (!IsUnit(srcWalker) && !IsDataItem(srcWalker))
							continue;

						MG_CHECK(srcWalker->m_FenceNumber < resultFenceNumer);
						MG_CHECK(!srcWalker->IsCacheItem());
//						MG_CHECK(srcWalker->HasInterest() || srcWalker->WasFailed(FR_MetaInfo));
						if (srcWalker->HasInterest())
						{
							SharedPtr<const TreeItem> holdInterest = srcWalker;
							auto dc = srcWalker->mc_DC;
							if (dc)
							{
								assert(dc->m_FenceNumber < resultFenceNumer);
								futureDataContainer.emplace_back(holdInterest, dc->CallCalcResult());
							}
						}
					}
				}
				fenceBell.set_value();
				return true;
			}
		);

		bellWaiter.get();

		reportD(SeverityTypeID::ST_MinorTrace, "FenceContainer Source Updates completed");
		if (msgData.size() != 1 || !msgData[0].empty())
			for (auto msg : msgData)
				reportD(SeverityTypeID::ST_MinorTrace, msg.AsRange());

		// check that all sub-items of result-holder are/become up-to-date or uninteresting
		for (const auto& fd: futureDataContainer)
		{
			auto srcItem = fd.first.get_ptr();
			auto dc = fd.second;
			if (dc->WasFailed(FR_MetaInfo))
			{
				srcItem->Fail(dc.get_ptr());
				continue;
			}
			WaitReady(srcItem->GetCurrUltimateItem());
			assert(CheckDataReady(srcItem->GetCurrUltimateItem()));
			if (dc->WasFailed(FR_Data))
				srcItem->Fail(dc.get_ptr());
		}

		reportD(SeverityTypeID::ST_MinorTrace, "FenceContainer completed calculations of all resulting items");
		if (msgData.size() != 1 || !msgData[0].empty())
			for (auto msg: msgData)
				reportD(SeverityTypeID::ST_MajorTrace, msg.AsRange());
		resultHolder->m_ReadAssets.emplace<fence_work_data>(std::move(futureDataContainer));
		resultHolder->SetIsInstantiated();
		return true;
	}
};

namespace {

	SubItemOperator subItemOperator;
	CheckOperator checkOperator;
	FenceContainerOperator fcOp;

}