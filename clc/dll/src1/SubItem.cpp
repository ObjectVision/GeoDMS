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

#include "CheckedDomain.h"
#include "OperGroups.h"
#include "TreeItemClass.h"

// *****************************************************************************
//										SubItemOperator
// *****************************************************************************

oper_arg_policy oap_SubItem[2] = { oper_arg_policy::calc_subitem_root, oper_arg_policy::calc_always };

SpecialOperGroup sog_SubItem("SubItem", 2, oap_SubItem, oper_policy::existing|oper_policy::dynamic_result_class);

struct SubItemOperator: public BinaryOperator
{
	typedef DataArray<SharedStr> Arg2Type;

	SubItemOperator()
		: BinaryOperator(&sog_SubItem,
				TreeItem::GetStaticClass(), 
				TreeItem::GetStaticClass(), Arg2Type::GetStaticClass())
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 2);
		const TreeItem* arg1 = args[0];
		dms_assert(arg1);
		dms_assert(arg1->IsCacheItem());
		if (!resultHolder) {
			dms_assert(!mustCalc);
			checked_domain<Void>(args[1]);

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
			dms_assert(CheckDataReady(resultHolder.GetOld()));
//			dms_assert(resultHolder->DataAllocated() || resultHolder->WasFailed(FR_Data));
/*
			if (!IsCalculatingOrReady(resultHolder.GetOld()) && !resultHolder->WasFailed(FR_Data))
			{
				dms_assert(!resultHolder->IsCacheItem());
				return resultHolder->PrepareData();
			}
*/
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

namespace {

	SubItemOperator subItemOperator;
	CheckOperator checkOperator;

}