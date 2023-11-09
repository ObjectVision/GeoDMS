// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "RLookup.ipp"

// *****************************************************************************
//                         ClassifyOperator
// *****************************************************************************

CommonOperGroup cog_classify("classify", oper_policy::dynamic_result_class);

template <typename V>
struct ClassifyOperator : SearchIndexOperatorImpl<V, classify_dispatcher>
{
   ClassifyOperator()
		: SearchIndexOperatorImpl<V, classify_dispatcher>(&cog_classify)
	{}
};

// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

namespace 
{
	tl_oper::inst_tuple_templ<typelists::numerics, ClassifyOperator> classifyInstances;
} // end anonymous namespace

