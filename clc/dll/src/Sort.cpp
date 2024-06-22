// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "mci/CompositeCast.h"
#include "set/DataCompare.h"
#include "utl/TypeListOper.h"
#include "RtcTypeLists.h"

#include "DataArray.h"
#include "DataItemClass.h"
#include "Unit.h"
#include "UnitClass.h"

// *****************************************************************************
//                         Sort
// *****************************************************************************

// REMOVE, TODO: AbstrSortOperator

template <class V>
class SortOperator : public UnaryOperator
{
	typedef DataArray<V> ArgumentType;
	typedef DataArray<V> ResultType;

public:
	// Override Operator
	SortOperator(AbstrOperGroup* gr)
		:	UnaryOperator(gr, ResultType::GetStaticClass(), ArgumentType::GetStaticClass()) 
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 1);

		const AbstrDataItem* adi = debug_cast<const AbstrDataItem*>(args[0]);
		dms_assert(adi);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(adi->GetAbstrDomainUnit(), adi->GetAbstrValuesUnit(), COMPOSITION(V) );

		if (mustCalc)
		{
			const ArgumentType *  di = const_array_cast<V>(adi);
			dms_assert(di);


			DataReadLock arg1Lock(adi);
			auto unsortedData = di->GetDataRead();

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);

			ResultType* result = mutable_array_cast<V>(resLock);
			auto resData = result->GetDataWrite();
			dms_assert(resData.size() == unsortedData.size());
			fast_copy(unsortedData.begin(), unsortedData.end(), resData.begin());

			std::sort(resData.begin(), resData.end(), DataCompare<V>());

			resLock.Commit();
		}
		return true;
	}
};


// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

namespace 
{

	CommonOperGroup cog_sort("sort");

	tl_oper::inst_tuple_templ<typelists::ranged_unit_objects, SortOperator, AbstrOperGroup* > sortOperators(&cog_sort);
} // end anonymous namespace


