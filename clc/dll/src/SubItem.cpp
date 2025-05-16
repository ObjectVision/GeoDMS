// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "ASync.h"
#include "act/any.h"
#include "dbg/SeverityType.h"
#include "utl/Quotes.h"
#include "LispRef.h"

#include "CheckedDomain.h"
#include "OperGroups.h"
#include "TreeItemClass.h"
#include "MoreDataControllers.h"

// *****************************************************************************
//										SubItemOperator
// *****************************************************************************

// normal phases in processing an operator
// -M find the operator group, determined by the operator name
// -M SRC->DST: determine signature, result meta-info, or result data for each of the arguments, determined by their corresponting OperArgPolicy
// -M SRC->DST: Signature and other result meta-info generation and determine suppliers: CreateResultCaller(...) -> CreateResult(..., ..., false);
// - 
// -M DST->SRC: Set target interests -> set iterest on m_ResultHolder->m_Data and SetSupplierIterest
// -M SRC->DST: Scheduling: Create OperationContexts -> Activate free OC's -> register running OC's -> Terminating OC's free waiters.
//	- W SRC->DST: Calculation: CalcResult  -> CreateResult(..., ..., true), called from running OC's

// SubItem determines the 2nd arg during interest-setting, during scheduling in order to determine
// which sub-item of a container should be calculated


// FenceContainer Creates a full mirror-tree as result meta-info, with Fence Numbers, but no supppliers
// at Calculation, it sets interest on source items that are mirrored by interesting sub-items, 
// schedules (which creates full mirror-trees at upstream Fences), and runs them, which starts by Calculation up stream Fences, all the way up and down; depth first dependency traversal.
//
// Intended purposes
// - Calc a sequential process in phases, to force summarization of intermediate results and thus not keeping large intermediate results for later summmarization
// - partition and serialize parallel work to avoid too much simultaneous intermediate data 
// 
// issues
// + Fence suppliers are always seen and numbered during meta-info info generation as this includes a mirror-tree
// - still red items in HESTIA:/TussenResultaten/StartJaar/StateNaAllocatie_Fenced
// - setting additional targets during or after calculations, see issue #902

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
			MG_CHECK(CheckDataReady(ultItem));
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
			assert(!mustCalc);
			resultHolder = arg1;
		}
		assert(resultHolder);

		if (mustCalc)
		{
			auto arg2A = AsDataItem(args[1]);
			assert(CheckDataReady(arg2A));
			assert(CheckDataReady(resultHolder.GetOld()));
			DataReadLock arg2Lock(arg2A);

			IntegrityCheckFailure(resultHolder.GetOld(), arg2A, [&resultHolder]() -> SharedStr
				{
					auto funcDC = dynamic_cast<FuncDC*>(&resultHolder);
					MG_CHECK(funcDC);
					auto condDC = funcDC->GetArgDC(1);
					assert(condDC);

					return AsFLispSharedStr(condDC->GetLispRef(), FormattingFlags::None);
				}
			);
			if (resultHolder->WasFailed())
				resultHolder.Fail(resultHolder.GetOld());

			SizeT nrFailures = arg2A->CountValues<Bool>(false);
			if (nrFailures)
			{
				SharedStr helperText;
				if (arg2A->GetAbstrDomainUnit()->GetCount() == 1)
				{
					assert(nrFailures == 1);
					auto funcDC = dynamic_cast<FuncDC*>(&resultHolder);
					MG_CHECK(funcDC);
					auto condDC = funcDC->GetArgDC(1);
					assert(condDC);

					helperText = mySSPrintF("%s is not true"
						, SingleQuote(AsFLispSharedStr(condDC->GetLispRef(), FormattingFlags::None).c_str())
					);
				}
				else
				{
					auto firstFailure = arg2A->FindPos<Bool>(false, 0);
					if (nrFailures > 1)
						helperText = mySSPrintF("%d elements failed, first failure at row %d"
							, nrFailures
							, firstFailure
						);
					else
						helperText = mySSPrintF("failure at row %d"
							, firstFailure
						);
				}

				// will be caught by SuspendibleUpdate who will Fail this.
				auto ultimate_item = resultHolder.GetUlt();
				resultHolder.Fail(mySSPrintF("[[%s]] %s : %s"
					, ultimate_item ? ultimate_item->GetFullCfgName().c_str() : ""
					, ICHECK_NAME
					, helperText
					)
				, FR_Validate
				); // will be caught by SuspendibleUpdate who will Fail this.

				assert(resultHolder.WasFailed(FR_Validate));
			}
		}
		return true;
	}
};

// *****************************************************************************
//										FenceOperator
// *****************************************************************************

#include "act/ActorVisitor.h"
#include "act/SupplierVisitFlag.h"

#include "CopyTreeContext.h"
#include "OperationContext.h"
#include "MoreDataControllers.h"
#include "SupplCache.h"
#include "UnitProcessor.h"

// FenceContainers are used to separate calculations into groups that are to be executed serially, sequentially and/or consequetively and NOT in parallel.
// All calculation steps behind the fence are to be completed before calculation steps in front of the fence, i.e. steps that use fenced results, are to be executed
// the carninality of fenced domains are assumed to be known as part of the fenced results and can be used in the schedule execution plan of the front items
// When any of a fence result consumers is scheduled, a fence is requested to execute as first part of the schedule execution of everything in front of the fence

TIC_CALL task_status DoWorkWhileWaitingFor(task_status* fenceStatus); // TODO: move to OperationContext.h

oper_arg_policy oap_Fence[2] = { oper_arg_policy::calc_subitem_root,  oper_arg_policy::calc_as_result };
SpecialOperGroup sog_FenceContainer(token::FenceContainer, 2, oap_Fence, oper_policy::dynamic_result_class);

struct FenceContainerOperator : BinaryOperator
{
	FenceContainerOperator()
		: BinaryOperator(&sog_FenceContainer, TreeItem::GetStaticClass(), TreeItem::GetStaticClass(), DataArray<SharedStr>::GetStaticClass())
	{}

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, LispPtr) const override
	{
		assert(args.size() == 2);

		MG_CHECK(IsMetaThread());

		SharedTreeItem sourceContainer = GetItem(args[0]);
		if (!resultHolder)
		{
			MG_CHECK(resultHolder.m_FenceNumber == 0);

			CopyTreeContext context(nullptr, sourceContainer, ""
				, DataCopyMode::MakeEndogenous | DataCopyMode::InFenceOperator | DataCopyMode::CopyReferredItems
			);

			resultHolder = context.Apply(); // might generate upstream FenceNumbers, hidden upstream


#if defined(MG_DEBUG)
			resultHolder->m_State.Set(actor_flag_set::AFD_PivotElem);
#endif

			auto resultFenceNumber = GetNextFenceNumber();
//			resultHolder->m_FenceNumber = resultFenceNumber;
			resultHolder.m_FenceNumber = resultFenceNumber;

			assert(sourceContainer->GetCurrFenceNumber() < resultFenceNumber);

			auto resultRoot = resultHolder.GetNew();
			for (auto resWalker = resultRoot; resWalker; resWalker = resultRoot->WalkCurrSubTree(resWalker))
			{
				auto srcItem = sourceContainer->FindItem(resWalker->GetRelativeName(resultHolder.GetNew()));
				if (!srcItem)
					continue;
				MG_CHECK(!srcItem->IsCacheItem());
				MG_CHECK(srcItem->GetCurrFenceNumber() < resultFenceNumber);

				if (resWalker != resultRoot) // avoid updating all fenced items before getting them
					resWalker->GetOrCreateSupplCache()->InitAt(srcItem);

				assert(!resWalker->HasInterest());

				if (IsUnit(resWalker))
					resWalker->SetReferredItem(srcItem);

				if (srcItem->WasFailed())
					resWalker->Fail(srcItem);

			}
		}
		assert(resultHolder);
		assert(!resultHolder->IsPassor());
	}

	// The set of source sub-items that should be updated and kept is to be determined after the scheduling of all its consumers
	// thus after scheduling this and before or during calculating this.
	// Therfore only when the processing of this Fence is executed, we'll look at which sub-items need to be copied or kept.
	// making and providing (shallow) copies of fenced data items is a mechanism to separate using a fence-result from using the fenced-item before the fence
	// i.e. 
	// 
	// container state { a; b; c := a+b; }  
	// container fence := FenceContainer(state, '...');
	// d := fence/a + fence/b; 
	// 
	// d := add(subitem(fence, 'a') + subitem(fence, 'b');
	// d should not be rewritten to state/a + state/b as that whould keep refs open that we want to close before completing of fence, but
	// of a of b, en/of hun domein suppliers zijn van fence, wordt bepaald op moment van fence execution;
	// op dat moment zijn de targets bekend en kan (re)scheduling bepaald worden; na completering van deze fence zijn de target domain bepaald en kan (re)scheduling van operaties na deze fence (beter) plaats vinden.


	bool CalcResult(TreeItemDualRef& resultHolder, ArgRefs args, std::vector<ItemReadLock> readLocks, Explain::Context * context) const override
	{
		assert(args.size() == 2);

		SharedTreeItem sourceContainer = GetItem(args[0]);

		// first, copy ranges of units ?
		auto resultRoot = resultHolder.GetNew();
		assert(resultRoot);

		using fence_member_pair = std::pair<SharedTreeItemInterestPtr, FutureData>;
		using fence_work_data = std::vector<fence_member_pair>;
		fence_work_data futureDataContainer;

		auto resultFenceNumber = resultHolder.m_FenceNumber;

		task_status fenceStatus = task_status::activated;
		std::exception_ptr fenceErrorPtr;

		auto resWalker = resultRoot;
		
		// now, collect all targets that this Fence should calculate and start a Main Thread action to do so.
		// this should be done before supplier Fences do this and after target collection and interest-setting of consuming Fences.
		// so each Fence Calculation causes an avalange of interest in targets in higher fences and then their calculation before this calculation starts
		PostMainThreadTask(resultFenceNumber, [sourceContainer, resultRoot, &resWalker, &fenceStatus, &fenceErrorPtr, &resultHolder, resultFenceNumber, &futureDataContainer](bool mustCancel)-> bool
			{
				fenceStatus = task_status::running;

				// work on exporting stuff from main thread
				if (!mustCancel)
				{
					try {
						if (s_CurrBlockedFenceNumber && s_CurrBlockedFenceNumber <= resultFenceNumber)
							throwErrorF("FenceContainer", "Invalid Recursion calling %s#%d from updating %s for %s#%d"
								, resultRoot->GetSourceName(), resultFenceNumber
								, s_CurrBlockedFenceItem->GetSourceName()
								, s_CurrFenceContainer->GetSourceName(), s_CurrBlockedFenceNumber
							);

						tmp_swapper<fence_number> lockFence(s_CurrBlockedFenceNumber, resultFenceNumber);
						s_CurrFenceContainer = resultRoot;
						for (; resWalker; resWalker = resultRoot->WalkCurrSubTree(resWalker))
						{
							assert(resWalker->GetCurrFenceNumber() == resultFenceNumber);

							auto resInterestPtr = resWalker->GetInterestPtrOrNull();
							if (!resInterestPtr)
								continue;

							auto srcItem = sourceContainer->FindItem(resWalker->GetRelativeName(resultRoot));
							assert(srcItem);

							MG_CHECK(!srcItem->IsCacheItem());

							s_CurrBlockedFenceItem = srcItem.get();
							if (!srcItem->SuspendibleUpdate(PS_Committed))
							{
								if (srcItem->WasFailed())
									resWalker->Fail(srcItem);
								if (SuspendTrigger::DidSuspend())
									return false;
							}
							assert(!SuspendTrigger::DidSuspend());

							if (!IsUnit(resWalker) && !IsDataItem(resWalker))
								continue;

							//						assert(resWalker->DoesHaveSupplInterest());

							auto dc = srcItem->mc_DC;
							if (dc)
							{
								auto fd = dc->CallCalcResult();
								if (SuspendTrigger::DidSuspend())
									return false;

								futureDataContainer.emplace_back(std::move(resInterestPtr), std::move(fd));
							}
						}
					}
					catch (Concurrency::task_canceled&)
					{
						throw;
					}
					catch (...)
					{
						fenceErrorPtr = std::current_exception();
						fenceStatus = task_status::exception;
						return true;
					}
				}
				fenceStatus = task_status::done;
				WakeUpJoiners();
				return true;
			}
		);

		{
//			using DecCountType = StaticMtDecrementalLock<decltype(s_NrRunningOperations), s_NrRunningOperations>;
//			DecCountType dontCountThisOperation;
			DoWorkWhileWaitingFor(&fenceStatus);
//			WakeUpJoiners();
//			bellWaiter.get();
		}

		// check that all sub-items of result-holder are/become up-to-date or uninteresting
		for (const auto& fd: futureDataContainer)
		{
			auto resItem = const_cast<TreeItem*>(fd.first.get_ptr());
			auto dc = fd.second;

			assert(resItem);
			assert(dc);

			if (!dc)
				continue;

			if (dc->WasFailed(FR_MetaInfo))
			{
				resItem->Fail(dc.get_ptr());
				continue;
			}
			WaitReady(dc->GetUlt());
			if (dc->WasFailed(FR_Data))
				resItem->Fail(dc.get_ptr());
			else
			{
				auto srcUltItem = dc->GetUlt();
				assert(srcUltItem);
				assert(CheckDataReady(srcUltItem));
				if (IsDataItem(resItem))
					AsDataItem(resItem)->m_DataObject = AsDataItem(srcUltItem)->m_DataObject;
				else
				{
					assert(IsUnit(srcUltItem));
					visit<typelists::all_unit_types>(AsUnit(srcUltItem), 
						[&resItem]<typename V>(const Unit<V>*srcUltUnit) { checked_valcast<Unit<V>*>(resItem)->m_RangeDataPtr = srcUltUnit->m_RangeDataPtr; }
					);
				}
				if (dc->WasFailed())
					resItem->Fail(dc.get_ptr());
				if (srcUltItem->WasFailed())
					resItem->Fail(srcUltItem);
			}
			resItem->SetIsInstantiated();
			resItem->StopSupplInterest();
		}

		DataReadLock msgLock(AsDataItem(args[1]));
		auto msgData = const_array_cast<SharedStr>(msgLock)->GetDataRead();

		if (msgData.size() != 1 || !msgData[0].empty())
			for (auto msg: msgData)
				reportF(SeverityTypeID::ST_MajorTrace, "FenceContainer(%d): %s", resultFenceNumber, SharedStr(msg));

		resultHolder->SetIsInstantiated();
		resultHolder->StopSupplInterest();

		return true;
	}
};

namespace {

	SubItemOperator subItemOperator;
	CheckOperator checkOperator;
	FenceContainerOperator fcOp;

}