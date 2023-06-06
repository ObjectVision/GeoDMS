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

#include "utl/MySPrintF.h"

#include "AbstrUnit.h"
#include "CopyTreeContext.h"
#include "DataArray.h"
#include "DataItemClass.h"
#include "MoreDataControllers.h"
#include "TreeItemClass.h"

// *****************************************************************************
//									Loop operator
// *****************************************************************************

class LoopOperator : public BinaryOperator
{
	using loop_count_t = UInt16;
	typedef TreeItem          Arg1Type; // container met te repeteren groep
	typedef DataArray<loop_count_t> Arg2Type; // max nr iterations; make it a unit

public:
	LoopOperator(AbstrOperGroup* gr)
		:	BinaryOperator(gr, TreeItem::GetStaticClass(), 
				Arg1Type::GetStaticClass(), 
				Arg2Type::GetStaticClass()
			) 
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 2);

		const Arg1Type* loopContents   = args[0];
		dms_assert(loopContents);

		const AbstrDataItem* maxNrIterParamA= AsDataItem(args[1]);
		DataReadLock lock(maxNrIterParamA);

		loop_count_t maxNrIter = const_array_cast<loop_count_t>(maxNrIterParamA)->GetIndexedValue(0);
		if (!IsDefined(maxNrIter))
			throwErrorD("Loop", "Nr iterations is undefined");

		if (!resultHolder)
			resultHolder = TreeItem::CreateCacheRoot();

		TreeItem* result = resultHolder;
		dms_assert(result);

		SharedStr lastIterName = SharedStr(loopContents->GetID());

		bool checkStopValue
			=	mustCalc 
			&&	(	loopContents->FindItem("stopValue")
				->	CheckObjCls(DataArray<Bool>::GetStaticClass())
				);

		for (loop_count_t i=0; i!= maxNrIter; ++i)
		{
			TreeItem* iter = result->CreateItem(GetTokenID_mt(mySSPrintF("iter%d", i).c_str()));
			dms_assert(iter);

			SharedStr expr = SharedStr( loopContents->GetID() );
			expr += "(";
			expr += mySSPrintF("UInt16(%d)", i);
			if (i>0)
			{
				expr += ",";
				expr += mySSPrintF("iter%d/nextValue", i-1);
			}
			expr += ")";

			iter->SetExpr(SharedStr(expr));

			if (checkStopValue && false) // TODO, NYI
			{
				const AbstrDataItem* stopParamA
					=	debug_cast<const AbstrDataItem*>(
							iter->FindItem("stopValue")->CheckObjCls(DataArray<Bool>::GetStaticClass())
						);

				if (stopParamA && GetValue<Bool>(stopParamA, 0))
					break;
			}
			lastIterName = SharedStr(iter->GetID());
		}
		TreeItem* lastIter = result->CreateItem(GetTokenID_mt("lastIter"));
		lastIter->SetExpr(SharedStr(lastIterName));
		result->SetIsInstantiated();

		return true;
	}
};

// *****************************************************************************
//									LoopNTV operator
// *****************************************************************************

static TokenID lastValueToken = GetTokenID_st("lastValue");


class LoopNTVOperator : public TernaryOperator
{
	typedef DataArray<SharedStr> Arg1Type; // namen van de loop-elementen
	typedef TreeItem             Arg2Type; // container met te repeteren groep
	typedef DataArray<SharedStr> Arg3Type;    // eerste waarde voor eerste parameter

public:
	LoopNTVOperator(AbstrOperGroup* gr)
		:	TernaryOperator(gr, TreeItem::GetStaticClass(), 
				Arg1Type::GetStaticClass()
			,	Arg2Type::GetStaticClass()
			,	Arg3Type::GetStaticClass()
			) 
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 3);

		const AbstrDataItem* iterNames     = debug_cast<const AbstrDataItem*>(args[0]);
		const Arg2Type*      loopContents  = debug_cast<const Arg2Type     *>(args[1]);
		const AbstrDataItem* currValueExpr = debug_cast<const AbstrDataItem*>(args[2]);

		dms_assert(iterNames);
		dms_assert(loopContents);
		dms_assert(currValueExpr);

		SharedStr currValueExprStr = currValueExpr->LockAndGetValue<SharedStr>(0);

		DataReadLock lock(iterNames);

		UInt32 nrIter = iterNames->GetAbstrDomainUnit()->GetCount();

		if (!resultHolder)
			resultHolder = TreeItem::CreateCacheRoot();

		TreeItem* result = resultHolder;
		dms_assert(result);

		for (UInt32 i=0; i!= nrIter; ++i)
		{
			SharedStr iterName = iterNames->GetValue<SharedStr>(i);
			CopyTreeContext ctc(
				resultHolder, 
				loopContents, 
				iterName.c_str(), 
				DataCopyMode::CopyExpr
			);
			TreeItem* iterItem = ctc.Apply();
			if (!iterItem)
				throwErrorF("Iterate", "Failed to instantiate %s", loopContents->GetSourceName().c_str());
			TreeItem* currValue = iterItem->_GetFirstSubItem();
			if (currValue)
				currValue->SetExpr(currValueExprStr);
			currValueExprStr = iterName + "/nextValue";
		}
		TreeItem* lastIter = result->CreateItem(lastValueToken);
		lastIter->SetExpr(currValueExprStr);
		result->SetIsInstantiated();

		return true;
	}
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

namespace 
{
	oper_arg_policy oap_Loop[2] = { oper_arg_policy::is_templ, oper_arg_policy::calc_always };
	oper_arg_policy oap_LoopNTV[3] = { oper_arg_policy::calc_always, oper_arg_policy::is_templ, oper_arg_policy::calc_never };

	SpecialOperGroup sopLoop("loop", 2, oap_Loop, oper_policy::dont_cache_result|oper_policy::calc_requires_metainfo);
	LoopOperator lo(&sopLoop);

	SpecialOperGroup sopLoopNTV("iterate", 3, oap_LoopNTV, oper_policy::dont_cache_result);
	LoopNTVOperator loNTV(&sopLoopNTV);

}

/******************************************************************************/

