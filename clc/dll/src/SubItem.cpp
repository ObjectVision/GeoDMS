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
			const TreeItem* subItem = arg1->GetCurrItem(subItemName).get();
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

namespace {

	SubItemOperator subItemOperator;

}