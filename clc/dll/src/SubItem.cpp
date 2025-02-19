// Copyright (C) 1998-2024 Object Vision b.v. 
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
		dms_assert(args.size() == 2);
		const TreeItem* arg1 = args[0];
		dms_assert(arg1);
		dms_assert(arg1->IsCacheItem());
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
			dms_assert(subItem->IsCacheItem());
			resultHolder = subItem;
		}
		dms_assert(resultHolder);

		if (mustCalc)
		{
			dms_assert(CheckDataReady(arg1));
			dms_assert(CheckDataReady(resultHolder.GetUlt()));
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
		dms_assert(args.size() == 2);
		const TreeItem* arg1 = args[0];
		dms_assert(arg1);
//		dms_assert(arg1->IsCacheItem());
		if (!resultHolder) {
			dms_assert(!mustCalc);
			resultHolder = arg1;
		}
		dms_assert(resultHolder);

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

//	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override;
	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, OperationContext* fc, LispPtr) const override
	{
		assert(args.size() == 2);
		assert(fc);

		auto sourceContainer = std::get<SharedTreeItem>(args[0]).get();
		if (!resultHolder)
		{
			CopyTreeContext context(nullptr, sourceContainer, ""
				, DataCopyMode::MakePassor | DataCopyMode::MakeEndogenous | DataCopyMode::InFenceOperator | DataCopyMode::NoRoot //  | DataCopyMode::CopyAlsoReferredItems);
			);
			context.m_FenceNumber = GetNextFenceNumber();

			resultHolder = context.Apply();
			resultHolder.m_FenceNumber = context.m_FenceNumber;
		}
		assert(resultHolder);

//		fence_work_data workData;

		auto resultRoot = resultHolder.GetNew();
		for (auto resWalker = resultRoot; resWalker; resWalker = resultRoot->WalkCurrSubTree(resWalker))
		{

			if (!IsDataItem(resWalker) && !IsUnit(resWalker))
				continue;

			auto srcItem = sourceContainer->FindItem(resWalker->GetRelativeName(resultHolder.GetNew()));
			MG_CHECK(!srcItem->IsCacheItem());

			if (srcItem->WasFailed())
				resWalker->Fail(srcItem);
			if(!resWalker->WasFailed(FR_MetaInfo))
			{
				assert(srcItem->mc_DC);
				assert(resWalker->mc_DC);
				fc->AddDependency(srcItem->GetCheckedDC());
			}
//			workData.emplace_back(resWalker, srcItem);
//			resWalker->SetDC(srcItem->GetFuncDC());

		}

//		resultHolder->m_ReadAssets.emplace<fence_work_data>(std::move(workData));
//		assert(resultHolder->m_ReadAssets.has_value());
	}
	bool CalcResult(TreeItemDualRef& resultHolder, ArgRefs args, std::vector<ItemReadLock> readLocks, OperationContext * fc, Explain::Context * context) const override
	{
		assert(args.size() == 2);
		auto sourceContainer = std::get<SharedTreeItem>(args[0]).get();
//		assert(resultHolder->m_ReadAssets.has_value());

//		const fence_work_data* workDataPtr = rtc::any::any_cast<fence_work_data>(&resultHolder->m_ReadAssets);

//		MG_CHECK(workDataPtr);

		// first, copy ranges of units ?
		auto resultRoot = resultHolder.GetNew();
		assert(resultRoot);

		using fence_member_pair = std::pair<SharedPtr<TreeItem>, FutureData>;
		using fence_work_data = std::vector<fence_member_pair>;

		fence_work_data futureDataContainer;

		std::promise<void> fenceBell;
		auto bellWaiter = fenceBell.get_future();
		auto resultFenceNumer = resultHolder.m_FenceNumber;
		SendMainThreadOper([resultRoot, resultFenceNumer, sourceContainer, &futureDataContainer]()
			{
				// schedule all requested results
				for (auto resWalker = resultRoot; resWalker; resWalker = resultRoot->WalkCurrSubTree(resWalker))
				{
					if (!IsUnit(resWalker) && !IsDataItem(resWalker))
						continue;
					SharedPtr<TreeItem> holdInterest = resWalker;
					auto dc = resWalker->mc_DC;
					assert(dc);
					if (dc)
					{
						assert(dc->m_FenceNumber < resultFenceNumer);
						futureDataContainer.emplace_back(holdInterest, dc->CallCalcResult());
					}
				}
			}
		);
		auto srcWalker = sourceContainer;
		std::vector<SharedTreeItemInterestPtr> interestHolders;

		PostMainThreadTask([sourceContainer, &srcWalker, &interestHolders, &fenceBell, &resultHolder](bool mustCancel)-> bool
			{
				assert(!SuspendTrigger::BlockerBase::IsBlocked());
				// work on exporting stuff from main thread
				if (!mustCancel)
				{
					for (; srcWalker; srcWalker = sourceContainer->WalkConstSubTree(srcWalker))
					{
						if (interestHolders.empty() || interestHolders.back() != srcWalker)
							interestHolders.emplace_back(srcWalker);

						if (!srcWalker->SuspendibleUpdate(PS_Committed))
						{
							if (srcWalker->WasFailed())
								resultHolder.GetNew()->Fail(srcWalker);
							if (SuspendTrigger::DidSuspend())
								return false;
						}
					}
				}
				fenceBell.set_value();
				return true;
			}
		);

		bellWaiter.get();

		// check that all sub-items of result-holder are/become up-to-date or uninteresting
		for (const auto& fd: futureDataContainer)
		{
			auto resWalker = fd.first.get_ptr();
			auto dc = fd.second;
			if (dc->WasFailed(FR_MetaInfo))
			{
				resWalker->Fail(dc.get_ptr());
				continue;
			}
			auto srcItem = GetItem(dc);
			if (IsUnit(resWalker))
			{
				auto resAbstrUnit = AsUnit(resWalker);
				resAbstrUnit->GetCount();
				CheckDataReady(resAbstrUnit->GetCurrRangeItem());
			}
			else if (IsDataItem(resWalker))
			{
//				srcItem->PrepareDataUsage(DrlType::Certain);

				DataReadLock readLock(AsDataItem(srcItem));
				AsDataItem(resWalker)->m_DataObject = readLock;
				assert(CheckDataReady(resWalker->GetCurrRangeItem()));
			}
			if (dc->WasFailed(FR_Data))
				resWalker->Fail(dc.get_ptr());
		}

		DataReadLock msgLock(AsDataItem(args[1]));
		auto msgData = const_array_cast<SharedStr>(msgLock)->GetDataRead();
		if (msgData.size() != 1 || !msgData[0].empty())
			for (auto msg: msgData)
				reportD(SeverityTypeID::ST_MajorTrace, msg.AsRange());
		resultHolder->SetIsInstantiated();
		return true;
	}
};

namespace {

	SubItemOperator subItemOperator;
	CheckOperator checkOperator;
	FenceContainerOperator fcOp;

}