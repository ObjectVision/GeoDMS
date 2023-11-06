// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

//#define MG_REDUCE_CODE_HACK

#include "Rlookup.ipp"

// *****************************************************************************
//                         RLookupOperator
// *****************************************************************************
namespace
{

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

	tl_oper::inst_tuple_templ<typelists::value_elements, RLookupOperator> rlookupInstances;

} // end anonymous namespace

/******************************************************************************/
