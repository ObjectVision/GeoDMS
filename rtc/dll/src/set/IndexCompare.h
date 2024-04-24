// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_SET_INDEXCOMPARE_H)
#define __RTC_SET_INDEXCOMPARE_H

#include "set/DataCompare.h"

// *****************************************************************************
//                      INDEX operations
// *****************************************************************************

template <typename CI, typename E = SizeT>
struct IndexCompareOper 
{
	using index_type = E;
	using value_array_begin_type = CI;
	using value_type = typename std::iterator_traits<value_array_begin_type>::value_type;

	IndexCompareOper(value_array_begin_type dataBegin)
		: m_DataBegin(dataBegin) 
	{}

	bool operator ()(index_type left, index_type right) const
	{ 
		return m_DataComp(m_DataBegin[left], m_DataBegin[right]);
	}

	value_array_begin_type  m_DataBegin;

	DataCompare<value_type> m_DataComp = {};
};



#endif // __RTC_SET_INDEXCOMPARE_H
