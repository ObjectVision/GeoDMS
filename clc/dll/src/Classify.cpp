#include "ClcPCH.h"
#pragma hdrstop

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

