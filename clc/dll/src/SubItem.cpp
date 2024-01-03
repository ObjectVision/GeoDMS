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

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

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
				//	throwError(ICHECK_NAME, "%s", iChecker->GetExpr().c_str()); // will be caught by SuspendibleUpdate who will Fail this.
				resultHolder.Fail(mySSPrintF(ICHECK_NAME ": %d element(s) failed",  nrFailures), FR_Validate); // will be caught by SuspendibleUpdate who will Fail this.
				dms_assert(resultHolder.WasFailed(FR_Validate));
//				return AVS_SuspendedOrFailed;
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
SpecialOperGroup sog_FenceContainer(token::FenceContainer, 2, oap_Fence, oper_policy::dynamic_result_class| oper_policy::existing);

using fence_member_pair = std::pair<SharedPtr<TreeItem>, InterestPtr<SharedPtr<const TreeItem>>>;
using fence_work_data = std::vector<fence_member_pair>;

TIC_CALL void IncSchedulingOperContextGroupNumber();

struct FenceContainerOperator : BinaryOperator
{
	FenceContainerOperator()
		: BinaryOperator(&sog_FenceContainer, TreeItem::GetStaticClass(), TreeItem::GetStaticClass(), DataArray<SharedStr>::GetStaticClass())
	{}

//	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override;
	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, OperationContext* fc, LispPtr) const override
	{
		dms_assert(args.size() == 2);
		auto sourceContainer = std::get<SharedTreeItem>(args[0]).get();
		if (!resultHolder) {
			CopyTreeContext context(nullptr, sourceContainer, ""
				, DataCopyMode::MakePassor | DataCopyMode::MakeEndogenous | DataCopyMode::InFenceOperator //  | DataCopyMode::CopyAlsoReferredItems);
			);
			resultHolder = context.Apply();
		}
		dms_assert(resultHolder);

		fence_work_data workData;

		auto resultRoot = resultHolder.GetNew();
		for (auto resWalker = resultRoot; resWalker; resWalker = resultRoot->WalkCurrSubTree(resWalker))
		{
			if (!IsDataItem(resWalker) && !IsUnit(resWalker))
				continue;

			auto srcItem = sourceContainer->FindItem(resWalker->GetRelativeName(resultHolder.GetNew()));
			
			fc->AddDependency(srcItem->GetCheckedDC());
			workData.emplace_back(resWalker, srcItem);
		}
		fc->m_MetaInfo = make_noncopyable_any<fence_work_data>(std::move(workData));
		IncSchedulingOperContextGroupNumber();
	}
	bool CalcResult(TreeItemDualRef& resultHolder, ArgRefs args, std::vector<ItemReadLock> readLocks, OperationContext * fc, Explain::Context * context) const override
	{
		dms_assert(args.size() == 2);
		auto sourceContainer = std::get<SharedTreeItem>(args[0]).get();

		const fence_work_data* workDataPtr = noncopyable_any_cast<fence_work_data>(&fc->m_MetaInfo);

		MG_CHECK(workDataPtr);

		// first, copy ranges of units ?
		for (const auto& fencePair: *workDataPtr)
		{	
			auto resWalker = fencePair.first.get_ptr();
			auto srcItem = fencePair.second.get_ptr();
			MG_CHECK(srcItem);
			if (srcItem->WasFailed(FR_Data))
			{
				resWalker->Fail(srcItem);
				continue;
			}
			if (IsUnit(srcItem))
			{
				auto srcAbstrUnit = AsUnit(srcItem->GetCurrUltimateItem());
				auto resAbstrUnit= AsUnit(resWalker);
				if (srcAbstrUnit->HasTiledRangeData())
				{
					visit<typelists::ranged_unit_objects>(srcAbstrUnit, [resAbstrUnit]<typename V>(const Unit<V>* srcUnit)
					{
						MG_CHECK(srcUnit);
						auto resUnit = dynamic_cast<Unit<V>*>(resAbstrUnit);
						MG_CHECK(resUnit);
						resUnit->m_RangeDataPtr.reset( srcUnit->m_RangeDataPtr.get() );
					});
						
				}
			}
			else if (IsDataItem(srcItem))
			{
				if (auto holdInterestIfAny = resWalker->GetInterestPtrOrNull())
				{
					DataReadLock readLock(AsDataItem(srcItem));
					AsDataItem(resWalker)->m_DataObject = readLock;
					assert(CheckDataReady(resWalker));
				}
			}
			if (srcItem->WasFailed())
				resWalker->Fail(srcItem);
			assert(resWalker->WasFailed(FR_Data) || CheckDataReady(resWalker));
		}

		fc->m_MetaInfo.reset();

		// check that all sub-items of result-holder are up-to-date or uninteresting

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