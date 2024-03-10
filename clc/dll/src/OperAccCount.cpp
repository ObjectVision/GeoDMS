// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "OperAccUniNum.h"


namespace 
{
	CommonOperGroup cogCount  ("count");
	CommonOperGroup cogCount08("count_uint8");
	CommonOperGroup cogCount16("count_uint16");
	CommonOperGroup cogCount32("count_uint32");
	CommonOperGroup cogCount64("count_uint64");

	template <typename UInt>
	struct count_func_types
	{
		template <typename T> using total_best = count_total_best<T, UInt>;
		template <typename T> using partial_best = count_partial_best<T, UInt>;
	};
	OperAccUniNum::AggrOperators<count_func_types<UInt32>::total_best, count_func_types<UInt32>::partial_best, typelists::ranged_unit_objects> s_CountOpers  (&cogCount);
	OperAccUniNum::AggrOperators<count_func_types<UInt8 >::total_best, count_func_types<UInt8 >::partial_best, typelists::ranged_unit_objects> s_CountOpers08(&cogCount08);
	OperAccUniNum::AggrOperators<count_func_types<UInt16>::total_best, count_func_types<UInt16>::partial_best, typelists::ranged_unit_objects> s_CountOpers16(&cogCount16);
	OperAccUniNum::AggrOperators<count_func_types<UInt32>::total_best, count_func_types<UInt32>::partial_best, typelists::ranged_unit_objects> s_CountOpers32(&cogCount32);
	OperAccUniNum::AggrOperators<count_func_types<UInt64>::total_best, count_func_types<UInt64>::partial_best, typelists::ranged_unit_objects> s_CountOpers64(&cogCount64);
}
