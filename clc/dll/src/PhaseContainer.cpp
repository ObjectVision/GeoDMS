// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// *****************************************************************************
//										PhaseContainerOperator
// *****************************************************************************

#include "act/ActorVisitor.h"
#include "act/SupplierVisitFlag.h"

#include "CopyTreeContext.h"
#include "LispTreeType.h"
#include "MoreDataControllers.h"
#include "OperationContext.h"
#include "SupplCache.h"
#include "TreeItemClass.h"
#include "UnitProcessor.h"
RTC_CALL bool s_IsDetectingIncInterest;

// PhaseContainers are used to separate calculations into groups that are to be executed serially, sequentially and/or consequetively and NOT in parallel.
// All calculation steps behind the phase are to be completed before calculation steps in front of the phase, i.e. steps that use fenced results, are to be executed
// the carninality of fenced domains are assumed to be known as part of the fenced results and can be used in the schedule execution plan of the front items
// When any of a phase result consumers is scheduled, a phase is requested to execute as first part of the schedule execution of everything in front of the phase

oper_arg_policy oap_Phase[2] = { oper_arg_policy::calc_never,  oper_arg_policy::calc_as_result };

SpecialOperGroup sog_PhaseContainer(token::PhaseContainer, 2, oap_Phase, oper_policy::dynamic_result_class | oper_policy::better_not_in_meta_scripting);

using fence_member_pair = std::pair<SharedTreeItemInterestPtr, FutureData>;
using fence_work_data = std::vector<fence_member_pair>;
using phase_resource = std::pair<fence_work_data, std::shared_ptr<TreeItem>>;

struct PhaseContainerOperator : BinaryOperator
{
	PhaseContainerOperator(AbstrOperGroup& operGroup)
		: BinaryOperator(&operGroup, TreeItem::GetStaticClass(), TreeItem::GetStaticClass(), DataArray<SharedStr>::GetStaticClass())
	{}

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, LispPtr) const override
	{
		assert(args.size() == 2);

		MG_CHECK(IsMetaThread());

		SharedTreeItem sourceContainer = make_shared_tree(GetItem(args[0]), existing_obj{});
		if (!resultHolder)
		{
			MG_CHECK(resultHolder.m_PhaseNumber == 0);

			CopyTreeContext context(nullptr, sourceContainer.get(), ""
				, DataCopyMode::MakeEndogenous | DataCopyMode::InFenceOperator | DataCopyMode::CopyReferredItems
			);

			resultHolder = context.Apply(); // might generate upstream FenceNumbers, hidden upstream

#if defined(MG_DEBUG)
			resultHolder->m_State.Set(actor_flag_set::AFD_PivotElem);
#endif

			auto resultPhaseNumber = GetNextPhaseNumber();
			resultHolder.m_PhaseNumber = resultPhaseNumber;

			assert(sourceContainer->GetCurrPhaseNumber() < resultPhaseNumber);

			auto resultRoot = resultHolder.GetNew();
			for (std::shared_ptr<TreeItem> resWalker = make_shared_tree(resultRoot, existing_obj{}); resWalker; resWalker = make_shared_tree(resultRoot->WalkCurrSubTree(resWalker.get()), existing_obj{}))
			{
				auto srcItem = sourceContainer->FindItem(resWalker->GetRelativeName(resultHolder.GetNew()));
				if (!srcItem)
					continue;
				MG_CHECK(!srcItem->IsCacheItem());

				if (srcItem->GetTSF(TSF_Categorical))
					resWalker->SetTSF(TSF_Categorical);

				if (auto srcUlt = srcItem->GetCurrUltimateItem())
					resWalker->m_StatusFlags.SetHasSortedValues(srcUlt->m_StatusFlags.HasSortedValues());

				auto srcPhaseNumber = srcItem->GetPhaseNumber();
				MG_CHECK(srcPhaseNumber < resultPhaseNumber);

				if (resWalker.get() != resultRoot) 
				{
					resWalker->GetOrCreateSupplCache()->InitAt(srcItem.get());
					/*
					std::vector< ActorCRef> srcSuppliers;
					srcSuppliers.emplace_back<const Actor*>(srcItem.get());

					srcItem->VisitSuppliers(SupplierVisitFlag::Update,
						MakeDerivedProcVisitor(
							[&srcSuppliers, resultPhaseNumber](const Actor* srcSuppl)
							{
								MG_CHECK(srcSuppl);
								MG_CHECK(srcSuppl->GetCurrPhaseNumber() < resultPhaseNumber);
								srcSuppliers.emplace_back(srcSuppl));
							}
						)
					);
					resWalker->GetOrCreateSupplCache()->InitAt(begin_ptr(srcSuppliers), end_ptr(srcSuppliers));
					*/
				}

				assert(!resWalker->HasInterest());

				if (IsUnit(resWalker))
					resWalker->SetReferredItem(srcItem.get());

				if (srcItem->WasFailed())
					resWalker->Fail(srcItem.get());
			}
		}
		assert(resultHolder);
		assert(!resultHolder->IsPassor());
	}

	// The set of source sub-items that should be updated and kept is to be determined after the scheduling of all its consumers
	// thus after scheduling this and before or during calculating this.
	// Therfore only when the processing of this Phase is executed, we'll look at which sub-items need to be copied or kept.
	// making and providing (shallow) copies of fenced data items is a mechanism to separate using a phase-result from using the fenced-item before the phase
	// i.e. 
	// 
	// container state { a; b; c := a+b; }  
	// container phase := PhaseContainer(state, '...');
	// d := phase/a + phase/b; 
	// 
	// d := add(subitem(phase, 'a') + subitem(phase, 'b');
	// d should not be rewritten to state/a + state/b as that whould keep refs open that we want to close before completing of phase, but
	// of a of b, en/of hun domein suppliers zijn van phase, wordt bepaald op moment van phase execution;
	// op dat moment zijn de targets bekend en kan (re)scheduling bepaald worden; na completering van deze phase zijn de target domain bepaald en kan (re)scheduling van operaties na deze phase (beter) plaats vinden.

	bool PreCalcUpdate(TreeItemDualRef& resultHolder, ArgRefs& args) const override
	{
		assert(args.size() == 2);

		SharedTreeItem sourceContainer = make_shared_tree(GetItem(args[0]), existing_obj{});

		auto resultPhaseNumber = resultHolder.m_PhaseNumber;
		auto resultRoot = resultHolder.GetNew();
		auto lockFence = tmp_swapper<phase_number>(s_CurrBlockedPhaseNumber, resultPhaseNumber);
		auto lockCurrPhaseContainer = tmp_swapper(s_CurrPhaseContainer, resultRoot);

		if (!resultRoot->m_ReadAssets.has_value())
		{
			resultRoot->m_ReadAssets.emplace<phase_resource>();
			resultRoot->m_ReadAssets.Get<phase_resource>().second = make_shared_tree(resultRoot, existing_obj{});
			resultRoot->SetTSF(TSF_ReadAssetsInterestScoped); // interest-scoped: StopInterest releases the phase_resource if the phase is abandoned before CalcResult completes
		}
		auto& futureDataContainer = resultRoot->m_ReadAssets.Get<phase_resource>().first;
		auto& resWalker = resultRoot->m_ReadAssets.Get<phase_resource>().second;

		// now, collect all targets that this Phase should calculate and start a Main Thread action to do so.

		// this should be done before supplier Fences do this and after target collection and interest-setting of consuming Fences.
		// so each Phase Calculation causes an avalange of interest in targets in higher fences and then their calculation before this calculation starts

		for (; resWalker; resWalker = make_shared_tree(resultRoot->WalkCurrSubTree(resWalker.get()), existing_obj{}))
		{
			assert(resWalker->GetCurrPhaseNumber() == resultPhaseNumber);

			auto resInterestPtr = resWalker->GetInterestPtrOrNull();
			if (!resInterestPtr)
				continue;

			auto srcItem = sourceContainer->FindItem(resWalker->GetRelativeName(resultRoot));
			assert(srcItem);
			assert(srcItem->GetCurrPhaseNumber() < resultPhaseNumber);
			MG_CHECK(!srcItem->IsCacheItem());

			s_CurrBlockedPhaseItem = srcItem.get();
			//*
			{
//				auto detectInterest = tmp_swapper(s_IsDetectingIncInterest, true);
				if (!srcItem->SuspendibleUpdate())
				{
					if (srcItem->WasFailed())
						resWalker->Fail(srcItem.get());
					if (SuspendTrigger::DidSuspend())
						return false;
				}
			}
			assert(!SuspendTrigger::DidSuspend());
			//*/
			if (IsUnit(resWalker) || IsDataItem(resWalker))
			{
				auto dc = srcItem->mc_DC;
				if (!dc)
					resWalker->Fail(mySSPrintF("PhaseContainer: Source %s has no calculation rule so its data cannot be collected", srcItem->GetSourceName()).c_str(), FailType::Data);
				else
				{
					auto fd = dc->CallCalcResult();
					if (SuspendTrigger::DidSuspend())
						return false;

					futureDataContainer.emplace_back(std::move(resInterestPtr), std::move(fd));
					continue; // defer processing and StopSupplInterest in WorkerThread below
				}
			}
			if (resWalker.get() != resultRoot)
				resWalker->StopSupplInterest();
		}
		assert(!SuspendTrigger::DidSuspend());
		assert(!resWalker);
		return true;
	}

	bool CalcResult(TreeItemDualRef& resultHolder, const ArgRefs& args, std::vector<ItemReadLock> readLocks, Explain::Context * context) const override
	{
		assert(args.size() == 2);

		SharedTreeItem sourceContainer = make_shared_tree(GetItem(args[0]), existing_obj{});

		// first, copy ranges of units ?
		auto resultRoot = resultHolder.GetNew();
		assert(resultRoot);

		auto resultPhaseNumber = resultHolder.m_PhaseNumber;
		assert(resultPhaseNumber > 0);

		MG_CHECK(resultRoot->m_ReadAssets.is_a<phase_resource>());	
		auto& futureDataContainer = resultRoot->m_ReadAssets.Get<phase_resource>().first;
		MG_CHECK(!resultRoot->m_ReadAssets.Get<phase_resource>().second);

		// check that all sub-items of result-holder are/become up-to-date or uninteresting
		for (const auto& fd: futureDataContainer)
		{
			auto resItem = const_cast<TreeItem*>(fd.first.get_ptr());
			auto dc = fd.second;

			assert(resItem);
			assert(dc);

			if (dc)
			{
				if (dc->WasFailed(FailType::MetaInfo))
					resItem->Fail(dc.get_ptr());
				else
				{
					// hold the source result and its ultimate item: interest does not block a meta-thread
					// DoInvalidate->Clear() on the source DC mid-phase, so an unowned GetUlt() raw can go null/dangle
					auto srcCurr = dc->GetCurr();
					auto srcUltItem = srcCurr ? srcCurr->GetCurrUltimateItem() : SharedTreeItem();
					if (!srcUltItem)
						resItem->Fail(mySSPrintF("PhaseContainer(%d): source result of %s is no longer available", resultPhaseNumber, resItem->GetSourceName()).c_str(), FailType::Data);
					else
					{
						WaitReady(srcUltItem.get());
						if (dc->WasFailed(FailType::Data))
							resItem->Fail(dc.get_ptr());
						else
						{
							assert(CheckDataReady(srcUltItem.get()));
							if (IsDataItem(resItem))
								AsDataItem(resItem)->m_DataObject = AsDataItem(srcUltItem.get())->m_DataObject;
							else
							{
								assert(IsUnit(srcUltItem.get()));
								visit<typelists::all_unit_types>(AsUnit(srcUltItem.get()),
									[&resItem]<typename V>(const Unit<V>*srcUltUnit) { checked_valcast<Unit<V>*>(resItem)->m_RangeDataPtr = srcUltUnit->m_RangeDataPtr; }
								);
							}
							if (dc->WasFailed())
								resItem->Fail(dc.get_ptr());
							if (srcUltItem->WasFailed())
								resItem->Fail(srcUltItem.get());
						}
					}
//					resItem->SetIsInstantiated();
				}
			}
			if (resItem != resultRoot)
				resItem->StopSupplInterest();
		}

		DataReadLock msgLock(AsDataItem(args[1]));
		auto msgData = const_array_cast<SharedStr>(msgLock)->GetDataRead();

		if (msgData.size() != 1 || !msgData[0].empty())
			for (auto msg: msgData)
				reportF(SeverityTypeID::ST_MajorTrace, "PhaseContainer(%d): %s", resultPhaseNumber, SharedStr(msg));

		resultHolder->SetIsInstantiated();
		resultHolder->StopSupplInterest();
		futureDataContainer.clear();
		resultRoot->m_ReadAssets.Clear();
		resultRoot->ClearTSF(TSF_ReadAssetsInterestScoped); // phase_resource consumed; keep the marker in sync

		return true;
	}
};

namespace {

	PhaseContainerOperator fcOp(sog_PhaseContainer);

}