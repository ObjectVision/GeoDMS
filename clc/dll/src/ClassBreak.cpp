// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

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
		assert(args.size() == 2);

		const AbstrDataItem* arg1A = debug_cast<const AbstrDataItem*>(args[0]);
		assert(arg1A);
		assert(arg1A->GetDynamicObjClass() == GetResultClass());

		const AbstrUnit* arg1Domain = arg1A->GetAbstrDomainUnit();
		assert(arg1Domain);
		const AbstrUnit* valuesUnit= arg1A->GetAbstrValuesUnit();
		assert(valuesUnit);

		const AbstrUnit* classUnit = AsUnit(args[1]);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(classUnit, valuesUnit);

		if (mustCalc)
		{
			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataReadLock  arg1Lock(arg1A);

			auto vcpc = GetCounts<Float64, CountType>(arg1A, MAX_VALUE(CountType));

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

