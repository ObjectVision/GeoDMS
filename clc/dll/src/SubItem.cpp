// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "dbg/SeverityType.h"
#include "utl/mySPrintF.h"
#include "utl/Quotes.h"
#include "LispRef.h"

#include "CheckedDomain.h"
#include "LispTreeType.h"
#include "MoreDataControllers.h"
#include "OperGroups.h"
#include "ParallelTiles.h"
#include "TreeItemClass.h"

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


// PhaseContainer Creates a full mirror-tree as result meta-info, with Phase Numbers, but no supppliers
// at Calculation, it sets interest on source items that are mirrored by interesting sub-items, 
// schedules (which creates full mirror-trees at upstream Fences), and runs them, which starts by Calculation up stream Fences, all the way up and down; depth first dependency traversal.
//
// Intended purposes
// - Calc a sequential process in phases, to force summarization of intermediate results and thus not keeping large intermediate results for later summmarization
// - partition and serialize parallel work to avoid too much simultaneous intermediate data 
// 
// issues
// + Phase suppliers are always seen and numbered during meta-info info generation as this includes a mirror-tree
// - still red items in HESTIA:/TussenResultaten/StartJaar/StateNaAllocatie_Fenced
// - setting additional targets during or after calculations, see issue #902

oper_arg_policy oap_SubItem[2] = { oper_arg_policy::calc_subitem_root, oper_arg_policy::calc_always };

SpecialOperGroup sog_SubItem(token::subitem, 2, oap_SubItem, oper_policy::existing|oper_policy::dynamic_result_class);

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
			if (subItem && subItem->IsCacheItem())
				subItem = subItem->GetCurrUltimateItem(); // "/nr_OrgEntity" -> "/org_rel"

			if (!subItem)
				GetGroup()->throwOperErrorF("Cannot find '%s' from '%s'",
					subItemName.c_str(),
					arg1->GetFullName().c_str()
				);
			assert(subItem->IsCacheItem());
			resultHolder = subItem;
		}
		assert(resultHolder);

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

// PhaseContainers are used to separate calculations into groups that are to be executed serially, sequentially and/or consequetively and NOT in parallel.
// All calculation steps behind the phase are to be completed before calculation steps in front of the phase, i.e. steps that use fenced results, are to be executed
// the carninality of fenced domains are assumed to be known as part of the fenced results and can be used in the schedule execution plan of the front items
// When any of a phase result consumers is scheduled, a phase is requested to execute as first part of the schedule execution of everything in front of the phase

oper_arg_policy oap_Phase[2] = { oper_arg_policy::calc_never,  oper_arg_policy::calc_as_result };
SpecialOperGroup sog_PhaseContainer(token::PhaseContainer, 2, oap_Phase, oper_policy::dynamic_result_class);

struct PhaseContainerOperator : BinaryOperator
{
	PhaseContainerOperator(AbstrOperGroup& operGroup)
		: BinaryOperator(&operGroup, TreeItem::GetStaticClass(), TreeItem::GetStaticClass(), DataArray<SharedStr>::GetStaticClass())
	{}

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, LispPtr) const override
	{
		assert(args.size() == 2);

		MG_CHECK(IsMetaThread());

		SharedTreeItem sourceContainer = GetItem(args[0]);
		if (!resultHolder)
		{
			MG_CHECK(resultHolder.m_PhaseNumber == 0);

			CopyTreeContext context(nullptr, sourceContainer, ""
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
			for (auto resWalker = resultRoot; resWalker; resWalker = resultRoot->WalkCurrSubTree(resWalker))
			{
				assert(!resWalker->HasInterest());

				auto srcItem = sourceContainer->FindItem(resWalker->GetRelativeName(resultHolder.GetNew()));
				if (!srcItem)
					continue;
				MG_CHECK(!srcItem->IsCacheItem());

				if (srcItem->GetTSF(TSF_Categorical))
					resWalker->SetTSF(TSF_Categorical);

				resWalker->m_StatusFlags.SetHasSortedValues(srcItem->GetCurrUltimateItem()->m_StatusFlags.HasSortedValues());

				auto srcPhaseNumber = srcItem->GetPhaseNumber();
				MG_CHECK(srcPhaseNumber < resultPhaseNumber);

				if (srcItem->WasFailed())
					resWalker->Fail(srcItem);

				if (!IsUnit(resWalker) && !IsDataItem(resWalker))
					continue;

				if (resWalker != resultRoot)
					resWalker->GetOrCreateSupplCache()->InitAt(srcItem);

				if (IsUnit(resWalker))
					resWalker->SetReferredItem(srcItem);

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


	bool CalcResult(TreeItemDualRef& resultHolder, ArgRefs args, std::vector<ItemReadLock> readLocks, Explain::Context * context) const override
	{
		assert(args.size() == 2);

		SharedTreeItem sourceContainer = GetItem(args[0]);

		// first, copy ranges of units ?
		auto resultRoot = resultHolder.GetNew();
		assert(resultRoot);

		auto resultPhaseNumber = resultHolder.m_PhaseNumber;
		assert(resultPhaseNumber > 0);

		std::exception_ptr fenceErrorPtr;

		
		// check that all sub-items of result-holder are/become up-to-date or uninteresting
		for (auto resWalker = resultRoot; resWalker; resWalker = resultRoot->WalkCurrSubTree(resWalker))
		{
			assert(resWalker->GetCurrPhaseNumber() == resultPhaseNumber);

			if (!IsUnit(resWalker) && !IsDataItem(resWalker))
				continue;

			auto resInterestPtr = resWalker->GetInterestPtrOrNull();
			if (!resInterestPtr)
				continue;

			auto srcItem = static_cast<const TreeItem*>( resWalker->GetSupplCache()->GetSupplier(0) );
			assert(srcItem->GetID() == resWalker->GetID());

			assert(srcItem->GetCurrPhaseNumber() < resultPhaseNumber);

			auto dc = srcItem->mc_DC;
			if (!dc)
				resWalker->Fail(mySSPrintF("PhaseContainer: Source %s has no calculation rule so its data cannot be collected", srcItem->GetSourceName()).c_str(), FR_Data);
			else if (dc->WasFailed(FR_MetaInfo))
				resWalker->Fail(dc.get_ptr());
			else 
			{
				WaitReady(dc->GetUlt());
				if (dc->WasFailed(FR_Data))
					resWalker->Fail(dc.get_ptr());
				else
				{
					auto srcUltItem = dc->GetUlt();
					assert(srcUltItem);
					assert(CheckDataReady(srcUltItem));
					if (IsDataItem(resWalker))
						AsDataItem(resWalker)->m_DataObject = AsDataItem(srcUltItem)->m_DataObject;
					else
					{
						assert(IsUnit(srcUltItem));
						visit<typelists::all_unit_types>(AsUnit(srcUltItem),
							[&resWalker]<typename V>(const Unit<V>*srcUltUnit) { checked_valcast<Unit<V>*>(resWalker)->m_RangeDataPtr = srcUltUnit->m_RangeDataPtr; }
						);
					}
					if (dc->WasFailed())
						resWalker->Fail(dc.get_ptr());
					if (srcUltItem->WasFailed())
						resWalker->Fail(srcUltItem);

					resWalker->SetIsInstantiated();
				}
			}
			resWalker->StopSupplInterest();
		}

		DataReadLock msgLock(AsDataItem(args[1]));
		auto msgData = const_array_cast<SharedStr>(msgLock)->GetDataRead();

		if (msgData.size() != 1 || !msgData[0].empty())
			for (auto msg: msgData)
				reportF(SeverityTypeID::ST_MajorTrace, "PhaseContainer(%d): %s", resultPhaseNumber, SharedStr(msg));

		resultHolder->SetIsInstantiated();
		resultHolder->StopSupplInterest();

		return true;
	}
};

namespace {

	SubItemOperator subItemOperator;
	CheckOperator checkOperator;
	PhaseContainerOperator fcOp(sog_PhaseContainer);
}