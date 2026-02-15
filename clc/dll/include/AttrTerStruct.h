// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__CLC_ATTRTERSTRUCT_H)
#define __CLC_ATTRTERSTRUCT_H

#include "AttrBinStruct.h"

// *****************************************************************************
//								TERNARY CHECKED FUNCTORS
// *****************************************************************************

template <typename TTerFunc>
struct ternary_func_checked: 
	ternary_func<
		typename TTerFunc::result_type, 
		typename TTerFunc::arg1_type, 
		typename TTerFunc::arg2_type, 
		typename TTerFunc::arg3_type
	>
{
	ternary_func_checked(const TTerFunc& f = TTerFunc()) : m_TerFunc(f) {}

	typename ternary_func_checked::result_type operator()(typename ternary_func_checked::arg1_cref arg1, typename ternary_func_checked::arg2_cref arg2, typename ternary_func_checked::arg3_cref arg3) const
	{ 
		return	(IsDefined(arg1) && IsDefined(arg2) && IsDefined(arg3))
			? m_TerFunc(arg1, arg2, arg3)
			: UNDEFINED_VALUE(typename ternary_func_checked::result_type);
	}
private:
	TTerFunc m_TerFunc;
};

template <typename TTerAssign>
struct ternary_assign_checked: 
	ternary_assign<
		typename TTerAssign::assignee_type, 
		typename TTerAssign::arg1_type, 
		typename TTerAssign::arg2_type, 
		typename TTerAssign::arg3_type
	>
{
	TTerAssign m_TerAssign;

	ternary_assign_checked(const TTerAssign& f = TTerAssign()) : m_TerAssign(f) {}

	void operator()(typename ternary_assign_checked::assignee_ref res, typename ternary_assign_checked::arg1_cref t1, typename ternary_assign_checked::arg2_cref t2, typename ternary_assign_checked::arg3_cref t3) const
	{ 
		if ( IsDefined(t1) && IsDefined(t2) && IsDefined(t3) )
			m_TerAssign(res, t1, t2, t3);
		else
			Assign(res, Undefined());
	}
};


#endif //!defined(__CLC_ATTRTERSTRUCT_H)