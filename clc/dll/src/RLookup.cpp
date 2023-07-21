#include "ClcPCH.h"
#pragma hdrstop

#include "Rlookup.ipp"

// *****************************************************************************
//                         RLookupOperator
// *****************************************************************************

CommonOperGroup cog_rlookup("rlookup", oper_policy::dynamic_result_class);

template <class V>
struct RLookupOperator : public SearchIndexOperatorImpl<V, rlookup_dispatcher>
{
	RLookupOperator()
		: SearchIndexOperatorImpl<V, rlookup_dispatcher>(&cog_rlookup)
	{}
};

// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

namespace 
{
#if defined(DMS_64)
	tl_oper::inst_tuple<typelists::value_elements, RLookupOperator<_>> rlookupInstances;
#else
	tl_oper::inst_tuple<typelists::fields, RLookupOperator<_>> rlookupInstances;
#endif
} // end anonymous namespace

/******************************************************************************/
