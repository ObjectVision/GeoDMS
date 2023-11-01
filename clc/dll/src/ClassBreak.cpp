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

#include "RtcTypeLists.h"
#include "dbg/DebugContext.h"
#include "utl/TypeListOper.h"

#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"

#include "CalcClassBreaks.h"

struct ClassifyFixedOperator: public BinaryOperator
{
	ClassifyFixedOperator(AbstrOperGroup* og, ClassBreakFunc classBreakFunc, const DataItemClass* dic)
		:	BinaryOperator(og, dic, dic, AbstrUnit::GetStaticClass())
		,	m_ClassBreakFunc(classBreakFunc)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 2);

		const AbstrDataItem* arg1A = debug_cast<const AbstrDataItem*>(args[0]);
		dms_assert(arg1A);
		dms_assert(arg1A->GetDynamicObjClass() == GetResultClass());

		const AbstrUnit* arg1Domain = arg1A->GetAbstrDomainUnit();
		dms_assert(arg1Domain);
		const AbstrUnit* valuesUnit= arg1A->GetAbstrValuesUnit();
		dms_assert(valuesUnit);

		const AbstrUnit* classUnit = AsUnit(args[1]);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(classUnit, valuesUnit);

		if (mustCalc)
		{
//			CDebugContextHandle timerContext("ClassifyFixedOperator", GetGroup()->GetName(), MG_DEBUG_CLASSBREAKS);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataReadLock  arg1Lock(arg1A);

//			timerContext.LogTime("LocksReady");

			auto vcpc = GetCounts(arg1A, MAX_VALUE(CountType));

//			timerContext.LogTime("Counting Done");

			m_ClassBreakFunc(res, vcpc.first, vcpc.second);
		}
		return true;
	}
private:
	ClassBreakFunc m_ClassBreakFunc;
};

// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

namespace 
{
	CommonOperGroup cog_EqualInterval("ClassifyEqualInterval", oper_policy::dynamic_result_class);
	CommonOperGroup cog_NZEqualInterval("ClassifyNonzeroEqualInterval", oper_policy::dynamic_result_class);
	CommonOperGroup cog_LogInterval("ClassifyLogInterval", oper_policy::dynamic_result_class);
	CommonOperGroup cog_EqualCount("ClassifyEqualCount", oper_policy::dynamic_result_class);
	CommonOperGroup cog_NZEqualCount("ClassifyNonzeroEqualCount", oper_policy::dynamic_result_class);
	CommonOperGroup cog_UniqueValues("ClassifyUniqueValues", oper_policy::dynamic_result_class);
	CommonOperGroup cog_CRJenksFisher("ClassifyJenksFisher", oper_policy::dynamic_result_class);
	CommonOperGroup cog_NZJenksFisher("ClassifyNonzeroJenksFisher", oper_policy::dynamic_result_class);

	template <typename V>
	struct ClassBreakOperators
	{

		ClassBreakOperators()
			:	cfoEI(&cog_EqualInterval, ClassifyEqualInterval, DataArray<V>::GetStaticClass())
			,	cfoNZEI(&cog_NZEqualInterval, ClassifyNZEqualInterval, DataArray<V>::GetStaticClass())
			,	cfoLI(&cog_LogInterval,   ClassifyLogInterval  , DataArray<V>::GetStaticClass())
			,	cfoEC(&cog_EqualCount,    ClassifyEqualCount   , DataArray<V>::GetStaticClass())
			,	cfoNZEC(&cog_NZEqualCount, ClassifyNZEqualCount, DataArray<V>::GetStaticClass())
			,	cfoUV(&cog_UniqueValues,  ClassifyUniqueValues , DataArray<V>::GetStaticClass())
			,	cfoNZJF(&cog_NZJenksFisher, ClassifyNZJenksFisher, DataArray<V>::GetStaticClass())
			,   cfoCRJF(&cog_CRJenksFisher, ClassifyCRJenksFisher, DataArray<V>::GetStaticClass())
		{}

		ClassifyFixedOperator cfoEI, cfoNZEI;
		ClassifyFixedOperator cfoLI;
		ClassifyFixedOperator cfoEC, cfoNZEC;
		ClassifyFixedOperator cfoUV;
		ClassifyFixedOperator cfoNZJF, cfoCRJF;
	};

	tl_oper::inst_tuple_templ<typelists::num_objects, ClassBreakOperators> classBreakInstances;
} // end anonymous namespace

/******************************************************************************/

