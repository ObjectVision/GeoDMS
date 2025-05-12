// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include <future>

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

oper_arg_policy oap_Fence[2] = { oper_arg_policy::subst_with_subitems,  oper_arg_policy::calc_as_result };
SpecialOperGroup sog_FenceContainer(token::FenceContainer, 2, oap_Fence, oper_policy::dynamic_result_class);

void AssignFenceNumber(const Actor* item, fence_number fn)
{
	assert(item);
	if (item->m_FenceNumber)
		return;
	item->m_FenceNumber = fn;

	if (auto dc = dynamic_cast<const DataController*>(item))
	{
		if (auto sdc = dynamic_cast<const SymbDC*>(dc))
			if (auto si = sdc->MakeResult())
				AssignFenceNumber(si, fn);
		else if (auto fc = dynamic_cast<const FuncDC*>(dc))
			for (DcRefListElem* argRef = fc->m_Args; argRef; argRef = argRef->m_Next)
				AssignFenceNumber(argRef->m_DC, fn);
		return;
	}

#if defined(MG_DEBUG_INTERESTSOURCE_LOGGING)


	if (auto ti = dynamic_cast<const TreeItem*>(item))
	{
		static TokenID sGenerate = GetTokenID_mt("Generate");

		if (ti->GetID() == sGenerate)
			reportF(SeverityTypeID::ST_MinorTrace, "Fence %d", fn);
/*
		if (ti->m_State.Get(actor_flag_set::AFD_PivotElem))
		{
			reportF(SeverityTypeID::ST_MinorTrace, "Fence %d", fn);
		}
*/
	}

#endif defined(MG_DEBUG_INTERESTSOURCE_LOGGING)
	try {
		VisitSupplProcImpl(item, SupplierVisitFlag::CalcAll, [fn](const Actor* suppl)
			{
				AssignFenceNumber(suppl, fn);
			}
		);
	}
	catch (...) {}
}

struct FenceContainerOperator : BinaryOperator
{
	FenceContainerOperator()
		: BinaryOperator(&sog_FenceContainer, TreeItem::GetStaticClass(), TreeItem::GetStaticClass(), DataArray<SharedStr>::GetStaticClass())
	{}

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, OperationContext* fc, LispPtr) const override
	{
		assert(args.size() == 2);
		assert(fc);

		MG_CHECK(IsMetaThread());

		auto sourceContainer = std::get<SharedTreeItem>(args[0]).get();
		if (!resultHolder)
		{
			MG_CHECK(resultHolder.m_FenceNumber == 0);

			CopyTreeContext context(nullptr, sourceContainer, ""
				, DataCopyMode::MakeEndogenous | DataCopyMode::InFenceOperator | DataCopyMode::NoRoot | DataCopyMode::CopyReferredItems
			);
			context.m_FenceNumber = GetNextFenceNumber();

			resultHolder = context.Apply();

#if defined(MG_DEBUG)
			resultHolder->m_State.Set(actor_flag_set::AFD_PivotElem);
#endif

			resultHolder->m_FenceNumber = context.m_FenceNumber;
			resultHolder.m_FenceNumber = context.m_FenceNumber;

			auto resultFenceNumber = resultHolder.m_FenceNumber;

			auto resultRoot = resultHolder.GetNew();
			for (auto resWalker = resultRoot; resWalker; resWalker = resultRoot->WalkCurrSubTree(resWalker))
			{
				MG_CHECK(resWalker->m_FenceNumber >= resultFenceNumber);

				auto srcItem = sourceContainer->FindItem(resWalker->GetRelativeName(resultHolder.GetNew()));
				if (!srcItem)
					continue;
				MG_CHECK(!srcItem->IsCacheItem());
				AssignFenceNumber(srcItem, resultFenceNumber);

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
	bool CalcResult(TreeItemDualRef& resultHolder, ArgRefs args, std::vector<ItemReadLock> readLocks, OperationContext * fc, Explain::Context * context) const override
	{
		assert(args.size() == 2);

		auto srcContainer = std::get<SharedTreeItem>(args[0]).get();

		// first, copy ranges of units ?
		auto resultRoot = resultHolder.GetNew();
		assert(resultRoot);

		using fence_member_pair = std::pair<SharedTreeItemInterestPtr, FutureData>;
		using fence_work_data = std::vector<fence_member_pair>;
		fence_work_data futureDataContainer;

		auto resultFenceNumer = resultHolder.m_FenceNumber;

		std::promise<void> fenceBell;
		auto bellWaiter = fenceBell.get_future();
		auto resWalker = resultRoot;
		
		PostMainThreadTask(resultFenceNumer, [srcContainer, resultRoot, &resWalker, &fenceBell, &resultHolder, resultFenceNumer, &futureDataContainer](bool mustCancel)-> bool
			{
				// work on exporting stuff from main thread
				if (!mustCancel)
				{
					if (s_CurrBlockedFenceNumber >= resultFenceNumer)
						throwErrorF("FenceContainer", "Invalid Recursion calling % s from updating %s for %s"
						,	resultRoot->GetFullName()
						,	s_CurrBlockedFenceItem->GetFullName()
						,	s_CurrFenceContainer->GetFullName()
						);
					tmp_swapper<fence_number> lockFence(s_CurrBlockedFenceNumber, resultFenceNumer);
					s_CurrFenceContainer = resultRoot;
					for (; resWalker; resWalker = resultRoot->WalkCurrSubTree(resWalker))
					{
						auto resInterestPtr = resWalker->GetInterestPtrOrNull();
						if (!resInterestPtr)
							continue;

						auto srcItem = srcContainer->FindItem(resWalker->GetRelativeName(resultRoot));
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
							auto dcInterest = dc->GetInterestPtrOrNull();
							assert(dcInterest); // follows from 
							if (dcInterest)
							{
								auto fd = dc->CallCalcResult();
								if (SuspendTrigger::DidSuspend())
									return false;

								futureDataContainer.emplace_back(std::move(resInterestPtr), std::move(fd));
							}
						}
					}
				}
				fenceBell.set_value();
				return true;
			}
		);

		{
			using DecCountType = StaticMtDecrementalLock<decltype(s_NrRunningOperations), s_NrRunningOperations>;
			DecCountType dontCountThisOperation;
			s_MtSemaphore.release();
			auto x = make_scoped_exit([] {s_MtSemaphore.acquire(); });

			WakeUpJoiners();
			bellWaiter.get();
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
				reportF(SeverityTypeID::ST_MajorTrace, "FenceContainer(%d): %s", resultFenceNumer, SharedStr(msg));

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